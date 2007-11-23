/*
 *		IP_MASQ_IRC irc masquerading module
 *
 *
 * Version:	@(#)ip_masq_irc.c 0.04   99/06/19
 *
 * Author:	Juan Jose Ciarlante
 *		
 * Additions:
 *  - recognize a few non-irc-II DCC requests (Oliver Wagner)
 *     DCC MOVE (AmIRC/DCC.MOVE; SEND with resuming)
 *     DCC SCHAT (AmIRC IDEA encrypted CHAT)
 *     DCC TSEND (AmIRC/PIRCH SEND without ACKs)
 * Fixes:
 *	Juan Jose Ciarlante	:  set NO_DADDR flag in ip_masq_new()
 *	Nigel Metheringham	:  Added multiple port support 
 *	Juan Jose Ciarlante	:  litl bits for 2.1
 *	Oliver Wagner 		:  more IRC cmds processing
 *	  <winmute@lucifer.gv.kotnet.org>
 *	Juan Jose Ciarlante	:  put new ms entry to listen()
 *	Scottie Shore		:  added support for clients that add extra args
 *	  <sshore@escape.ca>
 *	Scottie Shore		:  added support for mIRC DCC resume negotiation
 *	  <sshore@escape.ca>
 *	Juan Jose Ciarlante	:  src addr/port checking for better security (spotted by Michal Zalewski)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *	
 * Multiple Port Support
 *	The helper can be made to handle up to MAX_MASQ_APP_PORTS (normally 12)
 *	with the port numbers being defined at module load time.  The module
 *	uses the symbol "ports" to define a list of monitored ports, which can
 *	be specified on the insmod command line as
 *		ports=x1,x2,x3...
 *	where x[n] are integer port numbers.  This option can be put into
 *	/etc/conf.modules (or /etc/modules.conf depending on your config)
 *	where modload will pick it up should you use modload to load your
 *	modules.
 *
 * Insecure "back" data channel opening
 * 	The helper does some trivial checks when opening a new DCC data
 * 	channel. Use module parameter
 * 		insecure=1
 *	... to avoid this and get previous (pre 2.2.20) behaviour.
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <net/protocol.h>
#include <net/tcp.h>
#include <net/ip_masq.h>


/* 
 * List of ports (up to MAX_MASQ_APP_PORTS) to be handled by helper
 * First port is set to the default port.
 */
int ports[MAX_MASQ_APP_PORTS] = {6667}; /* I rely on the trailing items being set to zero */
struct ip_masq_app *masq_incarnations[MAX_MASQ_APP_PORTS];
/*
 *	Debug level
 */
#ifdef CONFIG_IP_MASQ_DEBUG
static int debug=0;
MODULE_PARM(debug, "i");
#endif

MODULE_PARM(ports, "1-" __MODULE_STRING(MAX_MASQ_APP_PORTS) "i");

static int insecure=0;
MODULE_PARM(insecure, "i");


/*
 * List of supported DCC protocols
 */

struct dccproto 
{
  char *match;
  int matchlen;
};

struct dccproto dccprotos[] = {
 { "SEND ", 5 },
 { "CHAT ", 5 },
 { "MOVE ", 5 },
 { "TSEND ", 6 },
 { "SCHAT ", 6 },
 { "ACCEPT ", 7 },
};

#define NUM_DCCPROTO (sizeof dccprotos / sizeof dccprotos[0])

#define MAXMATCHLEN 7

static int
masq_irc_init_1 (struct ip_masq_app *mapp, struct ip_masq *ms)
{
        MOD_INC_USE_COUNT;
        return 0;
}

static int
masq_irc_done_1 (struct ip_masq_app *mapp, struct ip_masq *ms)
{
        MOD_DEC_USE_COUNT;
        return 0;
}


/*
 * 	Ugly workaround [TM] --mummy ... why does this protocol sucks?
 *
 *	The <1024 check and same source address just raise the
 *	security "feeling" => they don't prevent a redirector listening
 *	in same src address at a higher port; you should protect 
 *	your internal network with ipchains output rules anyway
 */

static inline int masq_irc_out_check(const struct ip_masq *ms, __u32 data_saddr, __u16 data_sport) {
	int allow=1;

	IP_MASQ_DEBUG(1-debug, "masq_irc_out_check( s_addr=%d.%d.%d.%d, data_saddr=%d.%d.%d.%d, data_sport=%d",
			NIPQUAD(ms->saddr), NIPQUAD(data_saddr), ntohs(data_sport));

	/* 
	 * 	Ignore data channel back to other src addr, nor to port < 1024
	 */
	if (ms->saddr != data_saddr || ntohs(data_sport) < 1024) 
		allow=0;

	return allow;
}
int
masq_irc_out (struct ip_masq_app *mapp, struct ip_masq *ms, struct sk_buff **skb_p, __u32 maddr)
{
        struct sk_buff *skb;
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_limit;
	__u32 s_addr;
	__u32 s_port;	/* larger to allow strtoul() return value validation */
	struct ip_masq *n_ms;
	char buf[20];		/* "m_addr m_port" (dec base)*/
        unsigned buf_len;
	int diff;
        char *dcc_p, *addr_beg_p, *addr_end_p;

        skb = *skb_p;
	iph = skb->nh.iph;
        th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);
        data = (char *)&th[1];

        /*
	 *	Hunt irc DCC string, the _shortest_:
	 *
	 *	strlen("\1DCC CHAT chat AAAAAAAA P\1\n")=27
	 *	strlen("\1DCC SCHAT chat AAAAAAAA P\1\n")=28
	 *	strlen("\1DCC SEND F AAAAAAAA P S\1\n")=26
	 *	strlen("\1DCC MOVE F AAAAAAAA P S\1\n")=26
	 *	strlen("\1DCC TSEND F AAAAAAAA P S\1\n")=27
	 *	strlen("\1DCC ACCEPT F AAAAAAAA P S\1\n")=28
	 *		AAAAAAAAA: bound addr (1.0.0.0==16777216, min 8 digits)
	 *		P:         bound port (min 1 d )
	 *		F:         filename   (min 1 d )
	 *		S:         size       (min 1 d ) 
	 *		0x01, \n:  terminators
         */

        data_limit = skb->h.raw + skb->len;
        
	while (data < (data_limit - ( 22 + MAXMATCHLEN ) ) )
	{
		int i;
		if (memcmp(data,"\1DCC ",5))  {
			data ++;
			continue;
		}

		dcc_p = data;
		data += 5;     /* point to DCC cmd */

		for(i=0; i<NUM_DCCPROTO; i++)
		{
			/*
			 * go through the table and hunt a match string
			 */

			if( memcmp(data, dccprotos[i].match, dccprotos[i].matchlen ) == 0 )
			{
				data += dccprotos[i].matchlen;

				/*
				 *	skip next string.
				 */

				while( *data++ != ' ')

					/*
					 *	must still parse, at least, "AAAAAAAA P\1\n",
					 *      12 bytes left.
					 */
					if (data > (data_limit-12)) return 0;


				addr_beg_p = data;

				/*
				 *	client bound address in dec base
				 */

				s_addr = simple_strtoul(data,&data,10);
				if (*data++ !=' ')
					continue;

				/*
				 *	client bound port in dec base
				 */

				s_port = simple_strtoul(data,&data,10);
				addr_end_p = data;

				/*	Sanity checks 		*/
				if (!s_addr || !s_port || s_port > 65535)
					continue;

				/*	Prefer net order from now on 	*/
				s_addr = htonl(s_addr);
				s_port = htons(s_port);

				/* 	Simple validation 	*/
				if (!insecure && !masq_irc_out_check(ms, s_addr, s_port))
					/* We may just: return 0; */
					continue;

				/*	Do we already have a port open for this client?
				 *	If so, use it (for DCC ACCEPT)
				 */

				n_ms = ip_masq_out_get(IPPROTO_TCP,
						s_addr, s_port,
						0, 0);

				/*
				 *	If we didn't already have a port, we need to make one.
				 * 	We must set NO_DPORT and NO_DADDR because
				 *	connection is requested by another client.
				 */

				if (n_ms==NULL)
					n_ms = ip_masq_new(IPPROTO_TCP,
							maddr, 0,
							s_addr, s_port,
							0, 0,
							IP_MASQ_F_NO_DPORT|IP_MASQ_F_NO_DADDR);
				if (n_ms==NULL)
					return 0;

				/*
				 * Replace the old "address port" with the new one
				 */

				buf_len = sprintf(buf,"%u %u",
						ntohl(n_ms->maddr),ntohs(n_ms->mport));

				/*
				 * Calculate required delta-offset to keep TCP happy
				 */

				diff = buf_len - (addr_end_p-addr_beg_p);

				*addr_beg_p = '\0';
				IP_MASQ_DEBUG(1-debug, "masq_irc_out(): '%s' %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d detected (diff=%d)\n", dcc_p, 
					NIPQUAD(s_addr), htons(s_port), 
					NIPQUAD(n_ms->maddr), htons(n_ms->mport), 
					diff);

				/*
				 *	No shift.
				 */

				if (diff==0) {
					/*
					 * simple case, just copy.
					 */
					memcpy(addr_beg_p,buf,buf_len);
				} else {

					*skb_p = ip_masq_skb_replace(skb, GFP_ATOMIC,
							addr_beg_p, addr_end_p-addr_beg_p,
							buf, buf_len);
				}
				ip_masq_listen(n_ms);
				ip_masq_put(n_ms);
				return diff;
			}
		}
	}
	return 0;

}

int
masq_irc_in (struct ip_masq_app *mapp, struct ip_masq *ms, struct sk_buff **skb_p, __u32 maddr)
{
        struct sk_buff *skb;
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_limit;
	__u32 s_addr;
	__u16 s_port;
	struct ip_masq *n_ms;
	char buf[20];		/* "m_addr m_port" (dec base)*/
        unsigned buf_len;
	int diff;
        char *dcc_p, *addr_beg_p, *addr_end_p;

        skb = *skb_p;
	iph = skb->nh.iph;
        th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);
        data = (char *)&th[1];

        /*
	 *	Hunt irc DCC RESUME string:
	 *
	 *	strlen("\1DCC RESUME chat AAAAAAAA P\1\n")=29
	 *		AAAAAAAAA: bound addr (1.0.0.0==16777216, min 8 digits)
	 *		P:         bound port (min 1 d )
	 *		F:         filename   (min 1 d )
	 *		S:         size       (min 1 d ) 
	 *		0x01, \n:  terminators
         */

        data_limit = skb->h.raw + skb->len;
        
	while (data < (data_limit - 29 ) )
	{
		if (memcmp(data,"\1DCC RESUME ",12))  {
			data ++;
			continue;
		}

		dcc_p = data;
		data += 12;

		while( *data++ != ' ')
			/*
			 *	must still parse, at least, "AAAAAAAA P\1\n",
			 *      12 bytes left.
			 */
			if (data > (data_limit-12)) return 0;


		addr_beg_p = data;

		/*
		 *	masq bound address in dec base
		 */

		s_addr = simple_strtoul(data,&data,10);
		if (*data++ !=' ')
		continue;

		/*
		 *	masq bound port in dec base
		 */

		s_port = simple_strtoul(data,&data,10);
		addr_end_p = data;

		/*
		 *	Find the masq entry associated with this connection.
		 *	The entry should have no dest addr/port yet.
		 *	If there is no entry, return 0.
		 */

		n_ms = ip_masq_in_get(IPPROTO_TCP,
			0, 0,
			htonl(s_addr), htons(s_port));
					
		if (n_ms==NULL)
			return 0;

		/*
		 * Replace the outside address with the inside address
		 */

		buf_len = sprintf(buf,"%u %u",
			ntohl(n_ms->saddr),ntohs(n_ms->sport));

		/*
		 * Calculate required delta-offset to keep TCP happy
		 */

		diff = buf_len - (addr_end_p-addr_beg_p);

		*addr_beg_p = '\0';
		IP_MASQ_DEBUG(1-debug, "masq_irc_in(): '%s' %X:%X detected (diff=%d)\n", dcc_p, s_addr,s_port, diff);

		/*
		 *	No shift.
		 */

		if (diff==0) {
			/*
			 * simple case, just copy.
			 */
			memcpy(addr_beg_p,buf,buf_len);
		} else {
			*skb_p = ip_masq_skb_replace(skb, GFP_ATOMIC,
				addr_beg_p, addr_end_p-addr_beg_p,
				buf, buf_len);
		}

		return diff;

	}
	return 0;

}

/*
 *	Main irc object
 *     	You need 1 object per port in case you need
 *	to offer also other used irc ports (6665,6666,etc),
 *	they will share methods but they need own space for
 *	data. 
 */

struct ip_masq_app ip_masq_irc = {
        NULL,			/* next */
	"irc",			/* name */
        0,			/* type */
        0,			/* n_attach */
        masq_irc_init_1,	/* init_1 */
        masq_irc_done_1,	/* done_1 */
        masq_irc_out,		/* pkt_out */
        masq_irc_in		/* pkt_in */
};

/*
 * 	ip_masq_irc initialization
 */

__initfunc(int ip_masq_irc_init(void))
{
	int i, j;

	for (i=0; (i<MAX_MASQ_APP_PORTS); i++) {
		if (ports[i]) {
			if ((masq_incarnations[i] = kmalloc(sizeof(struct ip_masq_app),
							    GFP_KERNEL)) == NULL)
				return -ENOMEM;
			memcpy(masq_incarnations[i], &ip_masq_irc, sizeof(struct ip_masq_app));
			if ((j = register_ip_masq_app(masq_incarnations[i], 
						      IPPROTO_TCP, 
						      ports[i]))) {
				return j;
			}
			IP_MASQ_DEBUG(1-debug,
					"Irc: loaded support on port[%d] = %d\n",
			       i, ports[i]);
		} else {
			/* To be safe, force the incarnation table entry to NULL */
			masq_incarnations[i] = NULL;
		}
	}
	return 0;
}

/*
 * 	ip_masq_irc fin.
 */

int ip_masq_irc_done(void)
{
	int i, j, k;

	k=0;
	for (i=0; (i<MAX_MASQ_APP_PORTS); i++) {
		if (masq_incarnations[i]) {
			if ((j = unregister_ip_masq_app(masq_incarnations[i]))) {
				k = j;
			} else {
				kfree(masq_incarnations[i]);
				masq_incarnations[i] = NULL;
				IP_MASQ_DEBUG(1-debug, "Irc: unloaded support on port[%d] = %d\n",
				       i, ports[i]);
			}
		}
	}
	return k;
}


#ifdef MODULE
EXPORT_NO_SYMBOLS;

int init_module(void)
{
        if (ip_masq_irc_init() != 0)
                return -EIO;
        return 0;
}

void cleanup_module(void)
{
        if (ip_masq_irc_done() != 0)
                printk(KERN_INFO "ip_masq_irc: can't remove module");
}

#endif /* MODULE */
