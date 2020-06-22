/* $Id: pktgen.c,v 1.1.2.1 2002/03/01 12:15:05 davem Exp $
 * pktgen.c: Packet Generator for performance evaluation.
 *
 * Copyright 2001, 2002 by Robert Olsson <robert.olsson@its.uu.se>
 *                                 Uppsala University, Sweden
 *
 * A tool for loading the network with preconfigurated packets.
 * The tool is implemented as a linux module.  Parameters are output 
 * device, IPG (interpacket gap), number of packets, and whether
 * to use multiple SKBs or just the same one.
 * pktgen uses the installed interface's output routine.
 *
 * Additional hacking by:
 *
 * Jens.Laas@data.slu.se
 * Improved by ANK. 010120.
 * Improved by ANK even more. 010212.
 * MAC address typo fixed. 010417 --ro
 * Integrated.  020301 --DaveM
 * Added multiskb option 020301 --DaveM
 * Scaling of results. 020417--sigurdur@linpro.no
 *
 * See Documentation/networking/pktgen.txt for how to use this.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <net/checksum.h>
#include <asm/timex.h>

#define cycles()	((u32)get_cycles())

static char version[] __initdata = 
  "pktgen.c: v1.1 020418: Packet Generator for packet performance testing.\n";

/* Parameters */
static char pg_outdev[32], pg_dst[32];
static int pkt_size = ETH_ZLEN;
static int nfrags = 0;
static __u32 pg_count = 100000;  /* Default No packets to send */
static __u32 pg_ipg = 0;  /* Default Interpacket gap in nsec */
static int pg_multiskb = 0; /* Use multiple SKBs during packet gen. */

static int debug;
static int forced_stop;
static int pg_cpu_speed;
static int pg_busy;

static __u8 hh[14] = { 

  /* Overrun by /proc config  */

    0x00, 0x80, 0xC8, 0x79, 0xB3, 0xCB, 

    /* We fill in SRC address later */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00
};

static unsigned char *pg_dstmac = hh;
static char pg_result[512];

static struct net_device *pg_setup_inject(u32 *saddrp)
{
	struct net_device *odev;
	int p1, p2;
	u32 saddr;

	rtnl_lock();
	odev = __dev_get_by_name(pg_outdev);
	if (!odev) {
		sprintf(pg_result, "No such netdevice: \"%s\"", pg_outdev);
		goto out_unlock;
	}

	if (odev->type != ARPHRD_ETHER) {
		sprintf(pg_result, "Not ethernet device: \"%s\"", pg_outdev);
		goto out_unlock;
	}

	if (!netif_running(odev)) {
		sprintf(pg_result, "Device is down: \"%s\"", pg_outdev);
		goto out_unlock;
	}

	for (p1 = 6, p2 = 0; p1 < odev->addr_len + 6; p1++)
		hh[p1] = odev->dev_addr[p2++];

	saddr = 0;
	if (odev->ip_ptr) {
		struct in_device *in_dev = odev->ip_ptr;

		if (in_dev->ifa_list)
			saddr = in_dev->ifa_list->ifa_address;
	}
	atomic_inc(&odev->refcnt);
	rtnl_unlock();

	*saddrp = saddr;
	return odev;

out_unlock:
	rtnl_unlock();
	return NULL;
}

static u32 idle_acc_lo, idle_acc_hi;

static void nanospin(int pg_ipg)
{
	u32 idle_start, idle;

	idle_start = cycles();

	for (;;) {
		barrier();
		idle = cycles() - idle_start;
		if (idle * 1000 >= pg_ipg * pg_cpu_speed)
			break;
	}
	idle_acc_lo += idle;
	if (idle_acc_lo < idle)
		idle_acc_hi++;
}

static int calc_mhz(void)
{
	struct timeval start, stop;
	u32 start_s, elapsed;

	do_gettimeofday(&start);
	start_s = cycles();
	do {
		barrier();
		elapsed = cycles() - start_s;
		if (elapsed == 0)
			return 0;
	} while (elapsed < 1000 * 50000);
	do_gettimeofday(&stop);
	return elapsed/(stop.tv_usec-start.tv_usec+1000000*(stop.tv_sec-start.tv_sec));
}

static void cycles_calibrate(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		int res = calc_mhz();
		if (res > pg_cpu_speed)
			pg_cpu_speed = res;
	}
}

static struct sk_buff *fill_packet(struct net_device *odev, __u32 saddr)
{
	struct sk_buff *skb;
	__u8 *eth;
	struct udphdr *udph;
	int datalen, iplen;
	struct iphdr *iph;

	skb = alloc_skb(pkt_size + 64 + 16, GFP_ATOMIC);
	if (!skb) {
		sprintf(pg_result, "No memory");
		return NULL;
	}

	skb_reserve(skb, 16);

	/*  Reserve for ethernet and IP header  */
	eth = (__u8 *) skb_push(skb, 14);
	iph = (struct iphdr *)skb_put(skb, sizeof(struct iphdr));
	udph = (struct udphdr *)skb_put(skb, sizeof(struct udphdr));

	/*  Copy the ethernet header  */
	memcpy(eth, hh, 14);

	datalen = pkt_size - 14 - 20 - 8; /* Eth + IPh + UDPh */
	if (datalen < 0)
		datalen = 0;

	udph->source = htons(9);
	udph->dest = htons(9);
	udph->len = htons(datalen + 8); /* DATA + udphdr */
	udph->check = 0;  /* No checksum */

	iph->ihl = 5;
	iph->version = 4;
	iph->ttl = 3;
	iph->tos = 0;
	iph->protocol = IPPROTO_UDP; /* UDP */
	iph->saddr = saddr;
	iph->daddr = in_aton(pg_dst);
	iph->frag_off = 0;
	iplen = 20 + 8 + datalen;
	iph->tot_len = htons(iplen);
	iph->check = 0;
	iph->check = ip_fast_csum((void *) iph, iph->ihl);
	skb->protocol = __constant_htons(ETH_P_IP);
	skb->mac.raw = ((u8 *)iph) - 14;
	skb->dev = odev;
	skb->pkt_type = PACKET_HOST;

	if (nfrags <= 0) {
		skb_put(skb, datalen);
	} else {
		int frags = nfrags;
		int i;

		if (frags > MAX_SKB_FRAGS)
			frags = MAX_SKB_FRAGS;
		if (datalen > frags*PAGE_SIZE) {
			skb_put(skb, datalen-frags*PAGE_SIZE);
			datalen = frags*PAGE_SIZE;
		}

		i = 0;
		while (datalen > 0) {
			struct page *page = alloc_pages(GFP_KERNEL, 0);
			skb_shinfo(skb)->frags[i].page = page;
			skb_shinfo(skb)->frags[i].page_offset = 0;
			skb_shinfo(skb)->frags[i].size =
				(datalen < PAGE_SIZE ? datalen : PAGE_SIZE);
			datalen -= skb_shinfo(skb)->frags[i].size;
			skb->len += skb_shinfo(skb)->frags[i].size;
			skb->data_len += skb_shinfo(skb)->frags[i].size;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}

		while (i < frags) {
			int rem;

			if (i == 0)
				break;

			rem = skb_shinfo(skb)->frags[i - 1].size / 2;
			if (rem == 0)
				break;

			skb_shinfo(skb)->frags[i - 1].size -= rem;

			skb_shinfo(skb)->frags[i] = skb_shinfo(skb)->frags[i - 1];
			get_page(skb_shinfo(skb)->frags[i].page);
			skb_shinfo(skb)->frags[i].page = skb_shinfo(skb)->frags[i - 1].page;
			skb_shinfo(skb)->frags[i].page_offset += skb_shinfo(skb)->frags[i - 1].size;
			skb_shinfo(skb)->frags[i].size = rem;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}
	}

	return skb;
}


static void pg_inject(void)
{
	u32 saddr;
	struct net_device *odev;
	struct sk_buff *skb;
	struct timeval start, stop;
	u32 total, idle;
	u32 pc, lcount;
	char *p = pg_result;
	u32  pkt_rate, data_rate;
	char rate_unit;

	odev = pg_setup_inject(&saddr);
	if (!odev)
		return;

	skb = fill_packet(odev, saddr);
	if (skb == NULL)
		goto out_reldev;

	forced_stop = 0;
	idle_acc_hi = 0;
	idle_acc_lo = 0;
	pc = 0;
	lcount = pg_count;
	do_gettimeofday(&start);

	for(;;) {
		spin_lock_bh(&odev->xmit_lock);
		if (!netif_queue_stopped(odev)) {
			struct sk_buff *skb2 = skb;

			if (pg_multiskb)
				skb2 = skb_copy(skb, GFP_ATOMIC);
			else
				atomic_inc(&skb->users);
			if (!skb2)
				goto skip;
			if (odev->hard_start_xmit(skb2, odev)) {
				kfree_skb(skb2);
				if (net_ratelimit())
					printk(KERN_INFO "Hard xmit error\n");
			}
			pc++;
		}
	skip:
		spin_unlock_bh(&odev->xmit_lock);

		if (pg_ipg)
			nanospin(pg_ipg);
		if (forced_stop)
			goto out_intr;
		if (signal_pending(current))
			goto out_intr;

		if (--lcount == 0) {
			if (atomic_read(&skb->users) != 1) {
				u32 idle_start, idle;

				idle_start = cycles();
				while (atomic_read(&skb->users) != 1) {
					if (signal_pending(current))
						goto out_intr;
					schedule();
				}
				idle = cycles() - idle_start;
				idle_acc_lo += idle;
				if (idle_acc_lo < idle)
					idle_acc_hi++;
			}
			break;
		}

		if (netif_queue_stopped(odev) || current->need_resched) {
			u32 idle_start, idle;

			idle_start = cycles();
			do {
				if (signal_pending(current))
					goto out_intr;
				if (!netif_running(odev))
					goto out_intr;
				if (current->need_resched)
					schedule();
				else
					do_softirq();
			} while (netif_queue_stopped(odev));
			idle = cycles() - idle_start;
			idle_acc_lo += idle;
			if (idle_acc_lo < idle)
				idle_acc_hi++;
		}
	}

	do_gettimeofday(&stop);

	total = (stop.tv_sec - start.tv_sec) * 1000000 +
		stop.tv_usec - start.tv_usec;

	if (total == 0) total = 1;  /* division by zero protection */
 
	idle = (((idle_acc_hi<<20)/pg_cpu_speed)<<12)+idle_acc_lo/pg_cpu_speed;

	
	/* 
	   Rounding errors is around 1% on pkt_rate when total
	   is just over 100.000. When total is big (total >=
	   4.295 sec) pc need to be more than 430 to keep
	   rounding errors below 1%. Shouldn't be a problem:)
	   
	   */

	if (total < 100000) 
		pkt_rate = (pc*1000000)/total;
	else if (total < 0xFFFFFFFF/1000)        /* overflow protection: 2^32/1000 */
		pkt_rate = (pc*1000)/(total/1000);		  
	else if (total <  0xFFFFFFFF/100)     
		pkt_rate = (pc*100)/(total/10000);		  
	else if (total < 0xFFFFFFFF/10)     
		pkt_rate = (pc*10)/(total/100000);		  
	else
		pkt_rate = (pc/(total/1000000));
	
	data_rate = (pkt_rate*pkt_size);
	if (data_rate > 1024*1024 ) {   /* 10 MB/s */
		data_rate = data_rate / (1024*1024);
		rate_unit = 'M';
	} else {
		data_rate = data_rate / 1024;
		rate_unit = 'K';
	}
	
	p += sprintf(p, "OK: %u(c%u+d%u) usec, %u (%dbyte,%dfrags) %upps %u%cB/sec",
		     total, total-idle, idle,
		     pc, skb->len, skb_shinfo(skb)->nr_frags,
		     pkt_rate, data_rate, rate_unit
		);
	

out_relskb:
	kfree_skb(skb);
out_reldev:
        dev_put(odev);
	return;

out_intr:
	sprintf(pg_result, "Interrupted");
	goto out_relskb;
}

/* proc/net/pg */

static struct proc_dir_entry *pg_proc_ent = 0;
static struct proc_dir_entry *pg_busy_proc_ent = 0;

static int proc_pg_busy_read(char *buf , char **start, off_t offset,
			     int len, int *eof, void *data)
{
	char *p;
  
	p = buf;
	p += sprintf(p, "%d\n", pg_busy);
	*eof = 1;
  
	return p-buf;
}

static int proc_pg_read(char *buf , char **start, off_t offset,
			int len, int *eof, void *data)
{
	char *p;
	int i;
  
	p = buf;
	p += sprintf(p, "Params: count=%u pkt_size=%u frags %d ipg %u multiskb %d odev \"%s\" dst %s dstmac ",
		     pg_count, pkt_size, nfrags, pg_ipg, pg_multiskb,
		     pg_outdev, pg_dst);
	for (i = 0; i < 6; i++)
		p += sprintf(p, "%02X%s", pg_dstmac[i], i == 5 ? "\n" : ":");

	if (pg_result[0])
		p += sprintf(p, "Result: %s\n", pg_result);
	else
		p += sprintf(p, "Result: Idle\n");
	*eof = 1;

	return p - buf;
}

static int count_trail_chars(const char *buffer, unsigned int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++) {
		switch (buffer[i]) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '=':
			break;
		default:
			goto done;
		};
	}
done:
	return i;
}

static unsigned long num_arg(const char *buffer, unsigned long maxlen, 
			     unsigned long *num)
{
	int i = 0;

	*num = 0;
  
	for(; i < maxlen; i++) {
		if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
			*num *= 10;
			*num += buffer[i] -'0';
		} else
			break;
	}
	return i;
}

static int strn_len(const char *buffer, unsigned int maxlen)
{
	int i = 0;

	for(; i < maxlen; i++) {
		switch (buffer[i]) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
			goto done_str;
		default:
		};
	}
done_str:
	return i;
}

static int proc_pg_write(struct file *file, const char *buffer,
			 unsigned long count, void *data)
{
	int i = 0, max, len;
	char name[16], valstr[32];
	unsigned long value = 0;
  
	if (count < 1) {
		sprintf(pg_result, "Wrong command format");
		return -EINVAL;
	}
  
	max = count - i;
	i += count_trail_chars(&buffer[i], max);
  
	/* Read variable name */

	len = strn_len(&buffer[i], sizeof(name) - 1);
	memset(name, 0, sizeof(name));
	strncpy(name, &buffer[i], len);
	i += len;
  
	max = count -i;
	len = count_trail_chars(&buffer[i], max);
	i += len;

	if (debug)
		printk("pg: %s,%lu\n", name, count);

	/* Only stop is allowed when we are running */
  
	if (!strcmp(name, "stop")) {
		forced_stop = 1;
		if (pg_busy)
			strcpy(pg_result, "Stopping");
		return count;
	}

	if (pg_busy) {
		strcpy(pg_result, "Busy");
		return -EINVAL;
	}

	if (!strcmp(name, "pkt_size")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		if (value < 14+20+8)
			value = 14+20+8;
		pkt_size = value;
		sprintf(pg_result, "OK: pkt_size=%u", pkt_size);
		return count;
	}
	if (!strcmp(name, "frags")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		nfrags = value;
		sprintf(pg_result, "OK: frags=%u", nfrags);
		return count;
	}
	if (!strcmp(name, "ipg")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		pg_ipg = value;
		sprintf(pg_result, "OK: ipg=%u", pg_ipg);
		return count;
	}
	if (!strcmp(name, "multiskb")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		pg_multiskb = (value ? 1 : 0);
		sprintf(pg_result, "OK: multiskb=%d", pg_multiskb);
		return count;
	}
	if (!strcmp(name, "count")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		if (value != 0) {
			pg_count = value;
			sprintf(pg_result, "OK: count=%u", pg_count);
		} else 
			sprintf(pg_result, "ERROR: no point in sending 0 packets. Leaving count=%u", pg_count);
		return count;
	}
	if (!strcmp(name, "odev")) {
		len = strn_len(&buffer[i], sizeof(pg_outdev) - 1);
		memset(pg_outdev, 0, sizeof(pg_outdev));
		strncpy(pg_outdev, &buffer[i], len);
		i += len;
		sprintf(pg_result, "OK: odev=%s", pg_outdev);
		return count;
	}
	if (!strcmp(name, "dst")) {
		len = strn_len(&buffer[i], sizeof(pg_dst) - 1);
		memset(pg_dst, 0, sizeof(pg_dst));
		strncpy(pg_dst, &buffer[i], len);
		if(debug)
			printk("pg: dst set to: %s\n", pg_dst);
		i += len;
		sprintf(pg_result, "OK: dst=%s", pg_dst);
		return count;
	}
	if (!strcmp(name, "dstmac")) {
		char *v = valstr;
		unsigned char *m = pg_dstmac;

		len = strn_len(&buffer[i], sizeof(valstr) - 1);
		memset(valstr, 0, sizeof(valstr));
		strncpy(valstr, &buffer[i], len);
		i += len;

		for(*m = 0;*v && m < pg_dstmac + 6; v++) {
			if (*v >= '0' && *v <= '9') {
				*m *= 16;
				*m += *v - '0';
			}
			if (*v >= 'A' && *v <= 'F') {
				*m *= 16;
				*m += *v - 'A' + 10;
			}
			if (*v >= 'a' && *v <= 'f') {
				*m *= 16;
				*m += *v - 'a' + 10;
			}
			if (*v == ':') {
				m++;
				*m = 0;
			}
		}	  
		sprintf(pg_result, "OK: dstmac");
		return count;
	}

	if (!strcmp(name, "inject") || !strcmp(name, "start")) {
		MOD_INC_USE_COUNT;
		pg_busy = 1;
		strcpy(pg_result, "Starting");
		pg_inject();
		pg_busy = 0;
		MOD_DEC_USE_COUNT;
		return count;
	}

	sprintf(pg_result, "No such parameter \"%s\"", name);
	return -EINVAL;
}

static int __init pg_init(void)
{
	printk(version);
	cycles_calibrate();
	if (pg_cpu_speed == 0) {
		printk("pktgen: Error: your machine does not have working cycle counter.\n");
		return -EINVAL;
	}
	pg_proc_ent = create_proc_entry("net/pg", 0600, 0);
	if (!pg_proc_ent) {
		printk("pktgen: Error: cannot create net/pg procfs entry.\n");
		return -ENOMEM;
	}
	pg_proc_ent->read_proc = proc_pg_read;
	pg_proc_ent->write_proc = proc_pg_write;
	pg_proc_ent->data = 0;

	pg_busy_proc_ent = create_proc_entry("net/pg_busy", 0, 0);
	if (!pg_busy_proc_ent) {
		printk("pktgen: Error: cannot create net/pg_busy procfs entry.\n");
		remove_proc_entry("net/pg", NULL);
		return -ENOMEM;
	}
	pg_busy_proc_ent->read_proc = proc_pg_busy_read;
	pg_busy_proc_ent->data = 0;

	return 0;
}

static void __exit pg_cleanup(void)
{
	remove_proc_entry("net/pg", NULL);
	remove_proc_entry("net/pg_busy", NULL);
}

module_init(pg_init);
module_exit(pg_cleanup);

MODULE_AUTHOR("Robert Olsson <robert.olsson@its.uu.se");
MODULE_DESCRIPTION("Packet Generator tool");
MODULE_LICENSE("GPL");
MODULE_PARM(pg_count, "i");
MODULE_PARM(pg_ipg, "i");
MODULE_PARM(pg_cpu_speed, "i");
MODULE_PARM(pg_multiskb, "i");
