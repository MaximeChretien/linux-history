#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/videodev.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <asm/semaphore.h>

#include <linux/sound.h>
#include <linux/soundcard.h>
#include <asm/uaccess.h>


#define DEV_MAX  4

static int debug = 0;
static int devnr = -1;

MODULE_PARM(debug,"i");
MODULE_PARM(devnr,"i");

MODULE_AUTHOR("Gerd Knorr");
MODULE_LICENSE("GPL");

/* ----------------------------------------------------------------------- */

struct TVMIXER {
	struct i2c_client *dev;
	int minor;
	int count;
};

static struct TVMIXER devices[DEV_MAX];

static int tvmixer_adapters(struct i2c_adapter *adap);
static int tvmixer_clients(struct i2c_client *client);

/* ----------------------------------------------------------------------- */

static int mix_to_v4l(int i)
{
	int r;

	r = ((i & 0xff) * 65536 + 50) / 100;
	if (r > 65535) r = 65535;
	if (r <     0) r =     0;
	return r;
}

static int v4l_to_mix(int i)
{
	int r;

	r = (i * 100 + 32768) / 65536;
	if (r > 100) r = 100;
	if (r <   0) r =   0;
	return r | (r << 8);
}

static int v4l_to_mix2(int l, int r)
{
	r = (r * 100 + 32768) / 65536;
	if (r > 100) r = 100;
	if (r <   0) r =   0;
	l = (l * 100 + 32768) / 65536;
	if (l > 100) l = 100;
	if (l <   0) l =   0;
	return (r << 8) | l;
}

static int tvmixer_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct video_audio va;
	int left,right,ret,val = 0;
        struct TVMIXER *mix = file->private_data;
	struct i2c_client *client = mix->dev;

	if (NULL == client)
		return -ENODEV;
	
        if (cmd == SOUND_MIXER_INFO) {
                mixer_info info;
                strncpy(info.id, "tv card", sizeof(info.id));
                strncpy(info.name, client->dev.name, sizeof(info.name));
                info.modify_counter = 42 /* FIXME */;
                if (copy_to_user((void *)arg, &info, sizeof(info)))
                        return -EFAULT;
                return 0;
        }
        if (cmd == SOUND_OLD_MIXER_INFO) {
                _old_mixer_info info;
                strncpy(info.id, "tv card", sizeof(info.id));
                strncpy(info.name, client->dev.name, sizeof(info.name));
                if (copy_to_user((void *)arg, &info, sizeof(info)))
                        return -EFAULT;
                return 0;
        }
        if (cmd == OSS_GETVERSION)
                return put_user(SOUND_VERSION, (int *)arg);

	if (_SIOC_DIR(cmd) & _SIOC_WRITE)
		if (get_user(val, (int *)arg))
			return -EFAULT;

	/* read state */
	memset(&va,0,sizeof(va));
	client->driver->command(client,VIDIOCGAUDIO,&va);

	switch (cmd) {
	case MIXER_READ(SOUND_MIXER_RECMASK):
	case MIXER_READ(SOUND_MIXER_CAPS):
	case MIXER_READ(SOUND_MIXER_RECSRC):
	case MIXER_WRITE(SOUND_MIXER_RECSRC):
		ret = 0;
		break;

	case MIXER_READ(SOUND_MIXER_STEREODEVS):
		ret = SOUND_MASK_VOLUME;
		break;
	case MIXER_READ(SOUND_MIXER_DEVMASK):
		ret = SOUND_MASK_VOLUME;
		if (va.flags & VIDEO_AUDIO_BASS)
			ret |= SOUND_MASK_BASS;
		if (va.flags & VIDEO_AUDIO_TREBLE)
			ret |= SOUND_MASK_TREBLE;
		break;

	case MIXER_WRITE(SOUND_MIXER_VOLUME):
		left  = mix_to_v4l(val);
		right = mix_to_v4l(val >> 8);
		va.volume  = max(left,right);
		va.balance = (32768*min(left,right)) / (va.volume ? va.volume : 1);
		va.balance = (left<right) ? (65535-va.balance) : va.balance;
		client->driver->command(client,VIDIOCSAUDIO,&va);
		client->driver->command(client,VIDIOCGAUDIO,&va);
		/* fall throuth */
	case MIXER_READ(SOUND_MIXER_VOLUME):
		left  = (min(65536 - va.balance,32768) *
			 va.volume) / 32768;
		right = (min(va.balance,32768) *
			 va.volume) / 32768;
		ret = v4l_to_mix2(left,right);
		break;
		
	case MIXER_WRITE(SOUND_MIXER_BASS):
		va.bass = mix_to_v4l(val);
		client->driver->command(client,VIDIOCSAUDIO,&va);
		client->driver->command(client,VIDIOCGAUDIO,&va);
		/* fall throuth  */
	case MIXER_READ(SOUND_MIXER_BASS):
		ret = v4l_to_mix(va.bass);
		break;

	case MIXER_WRITE(SOUND_MIXER_TREBLE):
		va.treble = mix_to_v4l(val);
		client->driver->command(client,VIDIOCSAUDIO,&va);
		client->driver->command(client,VIDIOCGAUDIO,&va);
		/* fall throuth */
	case MIXER_READ(SOUND_MIXER_TREBLE):
		ret = v4l_to_mix(va.treble);
		break;

	default:
		return -EINVAL;
	}
	if (put_user(ret, (int *)arg))
		return -EFAULT;
	return 0;
}

static int tvmixer_open(struct inode *inode, struct file *file)
{
        int i, minor = minor(inode->i_rdev);
        struct TVMIXER *mix = NULL;
	struct i2c_client *client = NULL;

	for (i = 0; i < DEV_MAX; i++) {
		if (devices[i].minor == minor) {
			mix = devices+i;
			client = mix->dev;
			break;
		}
	}

	if (NULL == client)
		return -ENODEV;

	/* lock bttv in memory while the mixer is in use  */
	file->private_data = mix;
	if (client->adapter->owner)
		try_module_get(client->adapter->owner);
        return 0;
}

static int tvmixer_release(struct inode *inode, struct file *file)
{
	struct TVMIXER *mix = file->private_data;
	struct i2c_client *client;

	client = mix->dev;
	if (NULL == client) {
		return -ENODEV;
	}

	if (client->adapter->owner)
		module_put(client->adapter->owner);
	return 0;
}

static struct i2c_driver driver = {
	.owner           = THIS_MODULE,
	.name            = "tv card mixer driver",
        .id              = I2C_DRIVERID_TVMIXER,
	.flags           = I2C_DF_NOTIFY,
        .attach_adapter  = tvmixer_adapters,
        .detach_adapter  = tvmixer_adapters,
        .detach_client   = tvmixer_clients,
};

static struct file_operations tvmixer_fops = {
	.owner		= THIS_MODULE,
	.llseek         = no_llseek,
	.ioctl          = tvmixer_ioctl,
	.open           = tvmixer_open,
	.release        = tvmixer_release,
};

/* ----------------------------------------------------------------------- */

static int tvmixer_adapters(struct i2c_adapter *adap)
{
	struct list_head  *item;
	struct i2c_client *client;

	if (debug)
		printk("tvmixer: adapter %s\n",adap->dev.name);

	list_for_each(item,&adap->clients) {
		client = list_entry(item, struct i2c_client, list);
		tvmixer_clients(client);
	}
	return 0;
}

static int tvmixer_clients(struct i2c_client *client)
{
	struct video_audio va;
	int i,minor;

	/* TV card ??? */
	if (!(client->adapter->class & I2C_ADAP_CLASS_TV_ANALOG)) {
		/* ignore that one */
		if (debug)
			printk("tvmixer: %s is not a tv card\n",
			       client->adapter->dev.name);
		return -1;
	}
	if (debug)
		printk("tvmixer: debug: %s\n",client->dev.name);

	/* unregister ?? */
	for (i = 0; i < DEV_MAX; i++) {
		if (devices[i].dev == client) {
			/* unregister */
			unregister_sound_mixer(devices[i].minor);
			devices[i].dev = NULL;
			devices[i].minor = -1;
			printk("tvmixer: %s unregistered (#1)\n",client->dev.name);
			return 0;
		}
	}

	/* look for a free slot */
	for (i = 0; i < DEV_MAX; i++)
		if (NULL == devices[i].dev)
			break;
	if (i == DEV_MAX) {
		printk(KERN_WARNING "tvmixer: DEV_MAX too small\n");
		return -1;
	}

	/* audio chip with mixer ??? */
	if (NULL == client->driver->command) {
		if (debug)
			printk("tvmixer: %s: driver->command is NULL\n",
			       client->driver->name);
		return -1;
	}
	memset(&va,0,sizeof(va));
	if (0 != client->driver->command(client,VIDIOCGAUDIO,&va)) {
		if (debug)
			printk("tvmixer: %s: VIDIOCGAUDIO failed\n",
			       client->dev.name);
		return -1;
	}
	if (0 == (va.flags & VIDEO_AUDIO_VOLUME)) {
		if (debug)
			printk("tvmixer: %s: has no volume control\n",
			       client->dev.name);
		return -1;
	}

	/* everything is fine, register */
	if ((minor = register_sound_mixer(&tvmixer_fops,devnr)) < 0) {
		printk(KERN_ERR "tvmixer: cannot allocate mixer device\n");
		return -1;
	}

	devices[i].minor = minor;
	devices[i].count = 0;
	devices[i].dev   = client;
	printk("tvmixer: %s (%s) registered with minor %d\n",
	       client->dev.name,client->adapter->dev.name,minor);
	
	return 0;
}

/* ----------------------------------------------------------------------- */

static int tvmixer_init_module(void)
{
	int i;
	
	for (i = 0; i < DEV_MAX; i++)
		devices[i].minor = -1;
	i2c_add_driver(&driver);
	return 0;
}

static void tvmixer_cleanup_module(void)
{
	int i;
	
	i2c_del_driver(&driver);
	for (i = 0; i < DEV_MAX; i++) {
		if (devices[i].minor != -1) {
			unregister_sound_mixer(devices[i].minor);
			printk("tvmixer: %s unregistered (#2)\n",
			       devices[i].dev->dev.name);
		}
	}
}

module_init(tvmixer_init_module);
module_exit(tvmixer_cleanup_module);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
