/* airport.c 0.09b
 *
 * A driver for "Hermes" chipset based Apple Airport wireless
 * card.
 *
 * Copyright notice & release notes in file orinoco.c
 * 
 * Note specific to airport stub:
 * 
 *  0.05 : first version of the new split driver
 *  0.06 : fix possible hang on powerup, add sleep support
 */

#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/list.h>
#include <linux/adb.h>
#include <linux/pmu.h>

#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/irq.h>

#include "hermes.h"
#include "orinoco.h"

static char version[] __initdata = "airport.c 0.09b (Benjamin Herrenschmidt <benh@kernel.crashing.org>)";
MODULE_AUTHOR("Benjamin Herrenschmidt <benh@kernel.crashing.org>");
MODULE_DESCRIPTION("Driver for the Apple Airport wireless card.");
MODULE_LICENSE("Dual MPL/GPL");
EXPORT_NO_SYMBOLS;

struct airport {
	struct device_node* node;
	int irq_requested;
	int ndev_registered;
	int open;
	/* Common structure (fully included), see orinoco.h */
	struct orinoco_private priv;
};

#ifdef CONFIG_PMAC_PBOOK
static int airport_sleep_notify(struct pmu_sleep_notifier *self, int when);
static struct pmu_sleep_notifier airport_sleep_notifier = {
	airport_sleep_notify, SLEEP_LEVEL_NET,
};
#endif

/*
 * Function prototypes
 */

static struct orinoco_private* airport_attach(struct device_node *of_node);
static void airport_detach(struct orinoco_private* priv);
static int airport_open(struct net_device *dev);
static int airport_stop(struct net_device *dev);

/*
   A linked list of "instances" of the dummy device.  Each actual
   PCMCIA card corresponds to one device instance, and is described
   by one dev_link_t structure (defined in ds.h).

   You may not want to use a linked list for this -- for example, the
   memory card driver uses an array of dev_link_t pointers, where minor
   device numbers are used to derive the corresponding array index.
*/

static struct orinoco_private *airport_dev;

static int
airport_open(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;
	struct airport* card = (struct airport *)priv->card;
	int rc;

	netif_device_attach(dev);

	rc = orinoco_reset(priv);
	if (rc)
		airport_stop(dev);
	else {
		card->open = 1;
		netif_start_queue(dev);
	}

	return rc;
}

static int
airport_stop(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;
	struct airport* card = (struct airport *)priv->card;

	TRACE_ENTER(priv->ndev.name);

	netif_stop_queue(dev);
	orinoco_shutdown(priv);
	card->open = 0;

	TRACE_EXIT(priv->ndev.name);

	return 0;
}

#ifdef CONFIG_PMAC_PBOOK
static int
airport_sleep_notify(struct pmu_sleep_notifier *self, int when)
{
	struct orinoco_private *priv = airport_dev;
	struct hermes *hw = &priv->hw;
	struct net_device *dev = &priv->ndev;
	struct airport* card = (struct airport *)priv->card;
	int rc;
	
	if (! airport_dev)
		return PBOOK_SLEEP_OK;

	switch (when) {
	case PBOOK_SLEEP_REQUEST:
		break;
	case PBOOK_SLEEP_REJECT:
		break;
	case PBOOK_SLEEP_NOW:
		printk(KERN_INFO "%s: Airport entering sleep mode\n", dev->name);
		if (card->open) {
			netif_stop_queue(dev);
			orinoco_shutdown(priv);
			netif_device_detach(dev);
		}
		disable_irq(dev->irq);
		pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 0);
		break;
	case PBOOK_WAKE:
		printk(KERN_INFO "%s: Airport waking up\n", dev->name);
		pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 1);
		mdelay(200);
		hermes_reset(hw);
		rc = orinoco_reset(priv);
		if (rc)
			printk(KERN_ERR "airport: Error %d re-initing card !\n", rc);
		else if (card->open)
			netif_device_attach(dev);
		enable_irq(dev->irq);
		break;
	}
	return PBOOK_SLEEP_OK;
}
#endif /* CONFIG_PMAC_PBOOK */

static struct orinoco_private*
airport_attach(struct device_node* of_node)
{
	struct orinoco_private *priv;
	struct net_device *ndev;
	struct airport* card;
	hermes_t *hw;

	TRACE_ENTER("orinoco");

	if (of_node->n_addrs < 1 || of_node->n_intrs < 1) {
		printk(KERN_ERR "airport: wrong interrupt/addresses in OF tree\n");
		return NULL;
	}

	/* Allocate space for private device-specific data */
	card = kmalloc(sizeof(*card), GFP_KERNEL);
	if (!card) {
		printk(KERN_ERR "airport: can't allocate device datas\n");
		return NULL;
	}
	memset(card, 0, sizeof(*card));

	priv = &(card->priv);
	priv->card = card;
	ndev = &priv->ndev;
	hw = &priv->hw;
	card->node = of_node;

	if (!request_OF_resource(of_node, 0, " (airport)")) {
		printk(KERN_ERR "airport: can't request IO resource !\n");
		kfree(card);
		return NULL;
	}
	
	/* Setup the common part */
	if (orinoco_setup(priv) < 0) {
		release_OF_resource(of_node, 0);
		kfree(card);
		return NULL;
	}


	ndev->name[0] = '\0';	/* register_netdev will give us an ethX name */
	SET_MODULE_OWNER(ndev);

	/* Overrides */
	ndev->open = airport_open;
	ndev->stop = airport_stop;

	/* Setup interrupts & base address */
	ndev->irq = of_node->intrs[0].line;
	ndev->base_addr = (unsigned long)ioremap(of_node->addrs[0].address, 0x1000) - _IO_BASE;

	hermes_struct_init(hw, ndev->base_addr);
		
	/* Power up card */
	pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 1);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);

	/* Reset it before we get the interrupt */
	hermes_reset(hw);

	if (request_irq(ndev->irq, orinoco_interrupt, 0, "Airport", (void *)priv)) {
		printk(KERN_ERR "airport: Couldn't get IRQ %d\n", ndev->irq);
		goto failed;
	}
	card->irq_requested = 1;
	
	/* Tell the stack we exist */
	if (register_netdev(ndev) != 0) {
		printk(KERN_ERR "airport: register_netdev() failed\n");
		goto failed;
	}
	printk(KERN_DEBUG "airport: card registered for interface %s\n", ndev->name);
	card->ndev_registered = 1;

	/* And give us the proc nodes for debugging */
	if (orinoco_proc_dev_init(priv) != 0)
		printk(KERN_ERR "airport: Failed to create /proc node for %s\n",
		       ndev->name);

#ifdef CONFIG_PMAC_PBOOK
	pmu_register_sleep_notifier(&airport_sleep_notifier);
#endif
	return priv;
	
failed:
	airport_detach(priv);
	return NULL;
}				/* airport_attach */

/*======================================================================
  This deletes a driver "instance".  
  ======================================================================*/

static void
airport_detach(struct orinoco_private *priv)
{
	struct airport* card = (struct airport *)priv->card;

	/* Unregister proc entry */
	orinoco_proc_dev_cleanup(priv);

#ifdef CONFIG_PMAC_PBOOK
	pmu_unregister_sleep_notifier(&airport_sleep_notifier);
#endif
	if (card->ndev_registered)
		unregister_netdev(&priv->ndev);
	card->ndev_registered = 0;
	
	if (card->irq_requested)
		free_irq(priv->ndev.irq, priv);
	card->irq_requested = 0;

	if (priv->ndev.base_addr)
		iounmap((void *)(priv->ndev.base_addr + (unsigned long)_IO_BASE));
	priv->ndev.base_addr = 0;

	release_OF_resource(card->node, 0);
	
	pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 0);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);
	
	kfree(card);
}				/* airport_detach */

static int __init
init_airport(void)
{
	struct device_node* airport_node;

	printk(KERN_DEBUG "%s\n", version);

	MOD_INC_USE_COUNT;

	/* Lookup card in device tree */
	airport_node = find_devices("radio");
	if (airport_node && !strcmp(airport_node->parent->name, "mac-io"))
		airport_dev = airport_attach(airport_node);

	MOD_DEC_USE_COUNT;

	return airport_dev ? 0 : -ENODEV;
}

static void __exit
exit_airport(void)
{
	if (airport_dev)
		airport_detach(airport_dev);
	airport_dev = NULL;
}

module_init(init_airport);
module_exit(exit_airport);
