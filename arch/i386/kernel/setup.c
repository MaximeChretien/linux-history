/*
 *  linux/arch/i386/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  Enhanced CPU type detection by Mike Jagdis, Patrick St. Jean
 *  and Martin Mares, November 1997.
 *
 *  Force Cyrix 6x86(MX) and M II processors to report MTRR capability
 *  and Cyrix "coma bug" recognition by
 *      Zolt�n B�sz�rm�nyi <zboszor@mail.externet.hu> February 1999.
 * 
 *  Force Centaur C6 processors to report MTRR capability.
 *      Bart Hartgers <bart@etpmod.phys.tue.nl>, May 199.
 *
 *  Intel Mobile Pentium II detection fix. Sean Gilley, June 1999.
 *
 *  IDT Winchip tweaks, misc clean ups.
 *	Dave Jones <davej@suse.de>, August 1999
 *
 *	Added proper L2 cache detection for Coppermine
 *	Dragan Stancevic <visitor@valinux.com>, October 1999
 *
 *	Improved Intel cache detection.
 *	Dave Jones <davej@suse.de>, October 1999
 *
 *	Added proper Cascades CPU and L2 cache detection for Cascades
 *	and 8-way type cache happy bunch from Intel:^)
 *	Dragan Stancevic <visitor@valinux.com>, May 2000
 *
 *	Transmeta CPU detection.  H. Peter Anvin <hpa@zytor.com>, May 2000
 *
 *	Cleaned up get_model_name(), AMD_model(), added display_cacheinfo().
 *	Dave Jones <davej@suse.de>, September 2000
 *
 *	Added Cyrix III initial detection code
 *	Alan Cox <alan@redhat.com>, Septembr 2000
 *
 *      Improve cache size calculation
 *      Asit Mallick <asit.k.mallick@intel.com>, October 2000
 *      Andrew Ip <aip@turbolinux.com>, October 2000
 */

/*
 * This file handles the architecture-dependent parts of initialization
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/malloc.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/apm_bios.h>
#ifdef CONFIG_BLK_DEV_RAM
#include <linux/blk.h>
#endif
#include <asm/processor.h>
#include <linux/console.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/cobalt.h>
#include <asm/msr.h>
#include <asm/dma.h>

/*
 * Machine setup..
 */

char ignore_irq13 = 0;		/* set if exception 16 works */
struct cpuinfo_x86 boot_cpu_data = { 0, 0, 0, 0, -1, 1, 0, 0, -1 };

/*
 * Bus types ..
 */
int EISA_bus = 0;
int MCA_bus = 0;

/* for MCA, but anyone else can use it if they want */
unsigned int machine_id = 0;
unsigned int machine_submodel_id = 0;
unsigned int BIOS_revision = 0;
unsigned int mca_pentium_flag = 0;

/*
 * Setup options
 */
struct drive_info_struct { char dummy[32]; } drive_info;
struct screen_info screen_info;
struct apm_bios_info apm_bios_info;
struct sys_desc_table_struct {
	unsigned short length;
	unsigned char table[0];
};

unsigned char aux_device_present;

#ifdef CONFIG_BLK_DEV_RAM
extern int rd_doload;		/* 1 = load ramdisk, 0 = don't load */
extern int rd_prompt;		/* 1 = prompt for ramdisk, 0 = don't prompt */
extern int rd_image_start;	/* starting block # of image */
#endif

extern int root_mountflags;
extern int _etext, _edata, _end;
extern unsigned long cpu_khz;

/*
 * This is set up by the setup-routine at boot-time
 */
#define PARAM	((unsigned char *)empty_zero_page)
#define SCREEN_INFO (*(struct screen_info *) (PARAM+0))
#define EXT_MEM_K (*(unsigned short *) (PARAM+2))
#define ALT_MEM_K (*(unsigned long *) (PARAM+0x1e0))
#define APM_BIOS_INFO (*(struct apm_bios_info *) (PARAM+0x40))
#define DRIVE_INFO (*(struct drive_info_struct *) (PARAM+0x80))
#define SYS_DESC_TABLE (*(struct sys_desc_table_struct*)(PARAM+0xa0))
#define MOUNT_ROOT_RDONLY (*(unsigned short *) (PARAM+0x1F2))
#define RAMDISK_FLAGS (*(unsigned short *) (PARAM+0x1F8))
#define ORIG_ROOT_DEV (*(unsigned short *) (PARAM+0x1FC))
#define AUX_DEVICE_INFO (*(unsigned char *) (PARAM+0x1FF))
#define LOADER_TYPE (*(unsigned char *) (PARAM+0x210))
#define KERNEL_START (*(unsigned long *) (PARAM+0x214))
#define INITRD_START (*(unsigned long *) (PARAM+0x218))
#define INITRD_SIZE (*(unsigned long *) (PARAM+0x21c))
#define COMMAND_LINE ((char *) (PARAM+2048))
#define COMMAND_LINE_SIZE 256

#define RAMDISK_IMAGE_START_MASK  	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000	

#define BIOS_ENDBASE	0x9F000

#ifdef	CONFIG_VISWS
char visws_board_type = -1;
char visws_board_rev = -1;

#define	PIIX_PM_START		0x0F80

#define	SIO_GPIO_START		0x0FC0

#define	SIO_PM_START		0x0FC8

#define	PMBASE			PIIX_PM_START
#define	GPIREG0			(PMBASE+0x30)
#define	GPIREG(x)		(GPIREG0+((x)/8))
#define	PIIX_GPI_BD_ID1		18
#define	PIIX_GPI_BD_REG		GPIREG(PIIX_GPI_BD_ID1)

#define	PIIX_GPI_BD_SHIFT	(PIIX_GPI_BD_ID1 % 8)

#define	SIO_INDEX	0x2e
#define	SIO_DATA	0x2f

#define	SIO_DEV_SEL	0x7
#define	SIO_DEV_ENB	0x30
#define	SIO_DEV_MSB	0x60
#define	SIO_DEV_LSB	0x61

#define	SIO_GP_DEV	0x7

#define	SIO_GP_BASE	SIO_GPIO_START
#define	SIO_GP_MSB	(SIO_GP_BASE>>8)
#define	SIO_GP_LSB	(SIO_GP_BASE&0xff)

#define	SIO_GP_DATA1	(SIO_GP_BASE+0)

#define	SIO_PM_DEV	0x8

#define	SIO_PM_BASE	SIO_PM_START
#define	SIO_PM_MSB	(SIO_PM_BASE>>8)
#define	SIO_PM_LSB	(SIO_PM_BASE&0xff)
#define	SIO_PM_INDEX	(SIO_PM_BASE+0)
#define	SIO_PM_DATA	(SIO_PM_BASE+1)

#define	SIO_PM_FER2	0x1

#define	SIO_PM_GP_EN	0x80

static void
visws_get_board_type_and_rev(void)
{
	int raw;

	visws_board_type = (char)(inb_p(PIIX_GPI_BD_REG) & PIIX_GPI_BD_REG)
							 >> PIIX_GPI_BD_SHIFT;
/*
 * Get Board rev.
 * First, we have to initialize the 307 part to allow us access
 * to the GPIO registers.  Let's map them at 0x0fc0 which is right
 * after the PIIX4 PM section.
 */
	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_GP_DEV, SIO_DATA);	/* Talk to GPIO regs. */
    
	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_GP_MSB, SIO_DATA);	/* MSB of GPIO base address */

	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_GP_LSB, SIO_DATA);	/* LSB of GPIO base address */

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		/* Enable GPIO registers. */
    
/*
 * Now, we have to map the power management section to write
 * a bit which enables access to the GPIO registers.
 * What lunatic came up with this shit?
 */
	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_PM_DEV, SIO_DATA);	/* Talk to GPIO regs. */

	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_PM_MSB, SIO_DATA);	/* MSB of PM base address */
    
	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_PM_LSB, SIO_DATA);	/* LSB of PM base address */

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		/* Enable PM registers. */
    
/*
 * Now, write the PM register which enables the GPIO registers.
 */
	outb_p(SIO_PM_FER2, SIO_PM_INDEX);
	outb_p(SIO_PM_GP_EN, SIO_PM_DATA);
    
/*
 * Now, initialize the GPIO registers.
 * We want them all to be inputs which is the
 * power on default, so let's leave them alone.
 * So, let's just read the board rev!
 */
	raw = inb_p(SIO_GP_DATA1);
	raw &= 0x7f;	/* 7 bits of valid board revision ID. */

	if (visws_board_type == VISWS_320) {
		if (raw < 0x6) {
			visws_board_rev = 4;
		} else if (raw < 0xc) {
			visws_board_rev = 5;
		} else {
			visws_board_rev = 6;
	
		}
	} else if (visws_board_type == VISWS_540) {
			visws_board_rev = 2;
		} else {
			visws_board_rev = raw;
		}

		printk("Silicon Graphics %s (rev %d)\n",
			visws_board_type == VISWS_320 ? "320" :
			(visws_board_type == VISWS_540 ? "540" :
					"unknown"),
					visws_board_rev);
	}
#endif


static char command_line[COMMAND_LINE_SIZE] = { 0, };
       char saved_command_line[COMMAND_LINE_SIZE];
unsigned long i386_endbase __initdata =  0;

__initfunc(void setup_arch(char **cmdline_p,
	unsigned long * memory_start_p, unsigned long * memory_end_p))
{
	unsigned long memory_start, memory_end;
	char c = ' ', *to = command_line, *from = COMMAND_LINE;
	int len = 0;
	int read_endbase_from_BIOS = 1;

#ifdef CONFIG_VISWS
	visws_get_board_type_and_rev();
#endif

 	ROOT_DEV = to_kdev_t(ORIG_ROOT_DEV);
 	drive_info = DRIVE_INFO;
 	screen_info = SCREEN_INFO;
	apm_bios_info = APM_BIOS_INFO;
	if( SYS_DESC_TABLE.length != 0 ) {
		MCA_bus = SYS_DESC_TABLE.table[3] &0x2;
		machine_id = SYS_DESC_TABLE.table[0];
		machine_submodel_id = SYS_DESC_TABLE.table[1];
		BIOS_revision = SYS_DESC_TABLE.table[2];
	}
	aux_device_present = AUX_DEVICE_INFO;
	memory_end = (1<<20) + (EXT_MEM_K<<10);
#ifndef STANDARD_MEMORY_BIOS_CALL
	{
		unsigned long memory_alt_end = (1<<20) + (ALT_MEM_K<<10);
		/* printk(KERN_DEBUG "Memory sizing: %08x %08x\n", memory_end, memory_alt_end); */
		if (memory_alt_end > memory_end)
			memory_end = memory_alt_end;
	}
#endif

	memory_end &= PAGE_MASK;
#ifdef CONFIG_BLK_DEV_RAM
	rd_image_start = RAMDISK_FLAGS & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((RAMDISK_FLAGS & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((RAMDISK_FLAGS & RAMDISK_LOAD_FLAG) != 0);
#endif
	if (!MOUNT_ROOT_RDONLY)
		root_mountflags &= ~MS_RDONLY;
	memory_start = (unsigned long) &_end;
	init_task.mm->start_code = PAGE_OFFSET;
	init_task.mm->end_code = (unsigned long) &_etext;
	init_task.mm->end_data = (unsigned long) &_edata;
	init_task.mm->brk = (unsigned long) &_end;

	/* Save unparsed command line copy for /proc/cmdline */
	memcpy(saved_command_line, COMMAND_LINE, COMMAND_LINE_SIZE);
	saved_command_line[COMMAND_LINE_SIZE-1] = '\0';

	for (;;) {
		/*
		 * "mem=nopentium" disables the 4MB page tables.
		 * "mem=XXX[kKmM]" overrides the BIOS-reported
		 * memory size
		 */
		if (c == ' ' && *(const unsigned long *)from == *(const unsigned long *)"mem=") {
			if (to != command_line) to--;
			if (!memcmp(from+4, "nopentium", 9)) {
				from += 9+4;
				boot_cpu_data.x86_capability &= ~X86_FEATURE_PSE;
			} else {
				memory_end = simple_strtoul(from+4, &from, 0);
				if ( *from == 'K' || *from == 'k' ) {
					memory_end = memory_end << 10;
					from++;
				} else if ( *from == 'M' || *from == 'm' ) {
					memory_end = memory_end << 20;
					from++;
				}
			}
		}
		else if (c == ' ' && !memcmp(from, "endbase=", 8))
		{
			if (to != command_line) to--;
			i386_endbase = simple_strtoul(from+8, &from, 0);
			i386_endbase += PAGE_OFFSET;
			read_endbase_from_BIOS = 0;
		}
		c = *(from++);
		if (!c)
			break;
		if (COMMAND_LINE_SIZE <= ++len)
			break;
		*(to++) = c;
	}
	*to = '\0';
	*cmdline_p = command_line;

	if (read_endbase_from_BIOS)
	{
		/*
		 * The amount of available base memory is now taken from 
		 * WORD 40:13 (The BIOS EBDA pointer) in order to account for 
		 * some recent systems, where its value is smaller than the 
		 * 4K we blindly allowed before.
		 *
		 * (this was pointed out by Josef Moellers from
		 * Siemens Paderborn (Germany) ).
		 */
		i386_endbase = (*(unsigned short *)__va(0x413)*1024)&PAGE_MASK;
		
		if (!i386_endbase || i386_endbase > 0xA0000)
		{
			/* Zero is valid according to the BIOS weenies */
			if(i386_endbase)
			{
				printk(KERN_NOTICE "Ignoring bogus EBDA pointer %lX\n", 
					i386_endbase);
			}
			i386_endbase = BIOS_ENDBASE;
		}
		i386_endbase += PAGE_OFFSET;
	}

#define VMALLOC_RESERVE	(64 << 20)	/* 64MB for vmalloc */
#define MAXMEM	((unsigned long)(-PAGE_OFFSET-VMALLOC_RESERVE))

	if (memory_end > MAXMEM)
	{
		memory_end = MAXMEM;
		printk(KERN_WARNING "Warning only %ldMB will be used.\n",
			MAXMEM>>20);
	}

	memory_end += PAGE_OFFSET;
	*memory_start_p = memory_start;
	*memory_end_p = memory_end;

#ifdef CONFIG_SMP
	/*
	 *	Save possible boot-time SMP configuration:
	 */
	init_smp_config();
#endif

#ifdef CONFIG_BLK_DEV_INITRD
	if (LOADER_TYPE) {
		initrd_start = INITRD_START ? INITRD_START + PAGE_OFFSET : 0;
		initrd_end = initrd_start+INITRD_SIZE;
		if (initrd_end > memory_end) {
			printk("initrd extends beyond end of memory "
			    "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
			    initrd_end,memory_end);
			initrd_start = 0;
		}
	}
#endif

	/* request I/O space for devices used on all i[345]86 PCs */
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
	request_region(0xf0,0x10,"fpu");

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif
}


__initfunc(static int get_model_name(struct cpuinfo_x86 *c))
{
	unsigned int n, dummy, *v;

	/* 
	 * Actually we must have cpuid or we could never have
	 * figured out that this was AMD/Centaur/Cyrix/Transmeta
	 * from the vendor info :-).
	 */

	cpuid(0x80000000, &n, &dummy, &dummy, &dummy);
	if (n < 0x80000004)
		return 0;
	cpuid(0x80000001, &dummy, &dummy, &dummy, &(c->x86_capability));
	v = (unsigned int *) c->x86_model_id;
	cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	c->x86_model_id[48] = 0;
	
	return 1;
}


__initfunc (static void display_cacheinfo(struct cpuinfo_x86 *c))
{
	unsigned int n, dummy, ecx, edx;

	cpuid(0x80000000, &n, &dummy, &dummy, &dummy);

	if (n >= 0x80000005){
		cpuid(0x80000005, &dummy, &dummy, &ecx, &edx);
		printk("CPU: L1 I Cache: %dK  L1 D Cache: %dK\n",
			ecx>>24, edx>>24);
		c->x86_cache_size=(ecx>>24)+(edx>>24);
	}

	/* Yes this can occur - the CyrixIII just has a large L1 */
	if (n < 0x80000006)
		return;	/* No function to get L2 info */

	cpuid(0x80000006, &dummy, &dummy, &ecx, &edx);
	c->x86_cache_size = ecx>>16;

	/* AMD errata T13 (order #21922) */
	if(boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
		boot_cpu_data.x86 == 6 && 
		boot_cpu_data.x86_model== 3 &&
		boot_cpu_data.x86_mask == 0)
	{
		c->x86_cache_size = 64;
	}

	printk("CPU: L2 Cache: %dK\n", c->x86_cache_size);
}



__initfunc(static int amd_model(struct cpuinfo_x86 *c))
{
	u32 l, h;
	unsigned long flags;
	int mbytes = max_mapnr >> (20-PAGE_SHIFT);
	
	int r=get_model_name(c);
	
	/*
	 *	Now do the cache operations. 
	 */
	 
	switch(c->x86)
	{
		case 5:
			if( c->x86_model < 6 )
			{
				/* Anyone with a K5 want to fill this in */				
				break;
			}

			/* K6 with old style WHCR */
			if( c->x86_model < 8 ||
				(c->x86_model== 8 && c->x86_mask < 8))
			{
				/* We can only write allocate on the low 508Mb */
				if(mbytes>508)
					mbytes=508;
					
				rdmsr(0xC0000082, l, h);
				if((l&0x0000FFFF)==0)
				{		
					l=(1<<0)|((mbytes/4)<<1);
					save_flags(flags);
					__cli();
					__asm__ __volatile__ ("wbinvd": : :"memory");
					wrmsr(0xC0000082, l, h);
					restore_flags(flags);
					printk(KERN_INFO "Enabling old style K6 write allocation for %d Mb\n",
						mbytes);

				}
				break;
			}
			if (c->x86_model == 8 || c->x86_model == 9 || c->x86_model == 13)
			{
				/* The more serious chips .. */
				
				if(mbytes>4092)
					mbytes=4092;

				rdmsr(0xC0000082, l, h);
				if((l&0xFFFF0000)==0)
				{
					l=((mbytes>>2)<<22)|(1<<16);
					save_flags(flags);
					__cli();
					__asm__ __volatile__ ("wbinvd": : :"memory");
					wrmsr(0xC0000082, l, h);
					restore_flags(flags);
					printk(KERN_INFO "Enabling new style K6 write allocation for %d Mb\n",
						mbytes);
				}

				/*  Set MTRR capability flag if appropriate  */
				if((boot_cpu_data.x86_model == 13) ||
				   (boot_cpu_data.x86_model == 9) ||
				  ((boot_cpu_data.x86_model == 8) && 
				   (boot_cpu_data.x86_mask >= 8)))
					c->x86_capability |= X86_FEATURE_MTRR;
				break;
			}
			break;

		case 6:	/* An Athlon. We can trust the BIOS probably */
		{
			break;
		}
	}

	display_cacheinfo(c);
	return r;
}

__initfunc(static void intel_model(struct cpuinfo_x86 *c))
{
	unsigned int *v = (unsigned int *) c->x86_model_id;
	cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	c->x86_model_id[48] = 0;
	printk("CPU: %s\n", c->x86_model_id);
}
			

/*
 * Read Cyrix DEVID registers (DIR) to get more detailed info. about the CPU
 */
static inline void do_cyrix_devid(unsigned char *dir0, unsigned char *dir1)
{
	unsigned char ccr2, ccr3;

	/* we test for DEVID by checking whether CCR3 is writable */
	cli();
	ccr3 = getCx86(CX86_CCR3);
	setCx86(CX86_CCR3, ccr3 ^ 0x80);
	getCx86(0xc0);   /* dummy to change bus */

	if (getCx86(CX86_CCR3) == ccr3) {       /* no DEVID regs. */
		ccr2 = getCx86(CX86_CCR2);
		setCx86(CX86_CCR2, ccr2 ^ 0x04);
		getCx86(0xc0);  /* dummy */

		if (getCx86(CX86_CCR2) == ccr2) /* old Cx486SLC/DLC */
			*dir0 = 0xfd;
		else {                          /* Cx486S A step */
			setCx86(CX86_CCR2, ccr2);
			*dir0 = 0xfe;
		}
	}
	else {
		setCx86(CX86_CCR3, ccr3);  /* restore CCR3 */

		/* read DIR0 and DIR1 CPU registers */
		*dir0 = getCx86(CX86_DIR0);
		*dir1 = getCx86(CX86_DIR1);
	}
	sti();
}

/*
 * Cx86_dir0_msb is a HACK needed by check_cx686_cpuid/slop in bugs.h in
 * order to identify the Cyrix CPU model after we're out of setup.c
 */
unsigned char Cx86_dir0_msb __initdata = 0;

static char Cx86_model[][9] __initdata = {
	"Cx486", "Cx486", "5x86 ", "6x86", "MediaGX ", "6x86MX ",
	"M II ", "Unknown"
};
static char Cx486_name[][5] __initdata = {
	"SLC", "DLC", "SLC2", "DLC2", "SRx", "DRx",
	"SRx2", "DRx2"
};
static char Cx486S_name[][4] __initdata = {
	"S", "S2", "Se", "S2e"
};
static char Cx486D_name[][4] __initdata = {
	"DX", "DX2", "?", "?", "?", "DX4"
};
static char Cx86_cb[] __initdata = "?.5x Core/Bus Clock";
static char cyrix_model_mult1[] __initdata = "12??43";
static char cyrix_model_mult2[] __initdata = "12233445";

__initfunc(static void cyrix_model(struct cpuinfo_x86 *c))
{
	unsigned char dir0, dir0_msn, dir0_lsn, dir1 = 0;
	char *buf = c->x86_model_id;
	const char *p = NULL;

	do_cyrix_devid(&dir0, &dir1);

	Cx86_dir0_msb = dir0_msn = dir0 >> 4; /* identifies CPU "family"   */
	dir0_lsn = dir0 & 0xf;                /* model or clock multiplier */

	/* common case step number/rev -- exceptions handled below */
	c->x86_model = (dir1 >> 4) + 1;
	c->x86_mask = dir1 & 0xf;

	/* Now cook; the original recipe is by Channing Corn, from Cyrix.
	 * We do the same thing for each generation: we work out
	 * the model, multiplier and stepping.  Black magic included,
	 * to make the silicon step/rev numbers match the printed ones.
	 */
	 
	switch (dir0_msn) {
		unsigned char tmp;

	case 0: /* Cx486SLC/DLC/SRx/DRx */
		p = Cx486_name[dir0_lsn & 7];
		break;

	case 1: /* Cx486S/DX/DX2/DX4 */
		p = (dir0_lsn & 8) ? Cx486D_name[dir0_lsn & 5]
			: Cx486S_name[dir0_lsn & 3];
		break;

	case 2: /* 5x86 */
		Cx86_cb[2] = cyrix_model_mult1[dir0_lsn & 5];
		p = Cx86_cb+2;
		break;

	case 3: /* 6x86/6x86L */
		Cx86_cb[1] = ' ';
		Cx86_cb[2] = cyrix_model_mult1[dir0_lsn & 5];
		if (dir1 > 0x21) { /* 686L */
			Cx86_cb[0] = 'L';
			p = Cx86_cb;
			(c->x86_model)++;
		} else             /* 686 */
			p = Cx86_cb+1;
		/* Emulate MTRRs using Cyrix's ARRs. */
		c->x86_capability |= X86_FEATURE_MTRR;
		/* 6x86's contain this bug */
		c->coma_bug = 1;
		break;

	case 4: /* MediaGX/GXm */
		/*
		 *	Life sometimes gets weiiiiiiiird if we use this
		 *	on the MediaGX. So we turn it off for now. 
		 */
		
#ifdef CONFIG_PCI_QUIRKS
		/* It isnt really a PCI quirk directly, but the cure is the
		   same. The MediaGX has deep magic SMM stuff that handles the
		   SB emulation. It thows away the fifo on disable_dma() which
		   is wrong and ruins the audio. 
                   
		   Bug2: VSA1 has a wrap bug so that using maximum sized DMA 
		   causes bad things. According to NatSemi VSA2 has another
		   bug to do with 'hlt'. I've not seen any boards using VSA2
		   and X doesn't seem to support it either so who cares 8).
		   VSA1 we work around however.
		*/

		printk(KERN_INFO "Working around Cyrix MediaGX virtual DMA bug.\n");
                isa_dma_bridge_buggy = 2;
                  	                                                                     	        
#endif
		/* GXm supports extended cpuid levels 'ala' AMD */
		if (c->cpuid_level == 2) {
			get_model_name(c);  /* get CPU marketing name */
			c->x86_capability&=~X86_FEATURE_TSC;
			return;
		}
		else {  /* MediaGX */
			Cx86_cb[2] = (dir0_lsn & 1) ? '3' : '4';
			p = Cx86_cb+2;
			c->x86_model = (dir1 & 0x20) ? 1 : 2;
			c->x86_capability&=~X86_FEATURE_TSC;
		}
		break;

        case 5: /* 6x86MX/M II */
		if (dir1 > 7) dir0_msn++;  /* M II */
		else c->coma_bug = 1;      /* 6x86MX, it has the bug. */
		tmp = (!(dir0_lsn & 7) || dir0_lsn & 1) ? 2 : 0;
		Cx86_cb[tmp] = cyrix_model_mult2[dir0_lsn & 7];
		p = Cx86_cb+tmp;
        	if (((dir1 & 0x0f) > 4) || ((dir1 & 0xf0) == 0x20))
			(c->x86_model)++;
		/* Emulate MTRRs using Cyrix's ARRs. */
		c->x86_capability |= X86_FEATURE_MTRR;
		break;

	case 0xf:  /* Cyrix 486 without DEVID registers */
		switch (dir0_lsn) {
		case 0xd:  /* either a 486SLC or DLC w/o DEVID */
			dir0_msn = 0;
			p = Cx486_name[(c->hard_math) ? 1 : 0];
			break;

		case 0xe:  /* a 486S A step */
			dir0_msn = 0;
			p = Cx486S_name[0];
			break;
		break;
		}

	default:  /* unknown (shouldn't happen, we know everyone ;-) */
		dir0_msn = 7;
		break;
	}
	strcpy(buf, Cx86_model[dir0_msn & 7]);
	if (p) strcat(buf, p);
	return;
}

__initfunc(static void transmeta_model(struct cpuinfo_x86 *c))
{
	unsigned int cap_mask, uk, max, dummy;
	unsigned int cms_rev1, cms_rev2;
	unsigned int cpu_rev, cpu_freq, cpu_flags;
	char cpu_info[65];

	get_model_name(c);	/* Same as AMD/Cyrix */
	display_cacheinfo(c);

	/* Print CMS and CPU revision */
	cpuid(0x80860000, &max, &dummy, &dummy, &dummy);
	if ( max >= 0x80860001 ) {
		cpuid(0x80860001, &dummy, &cpu_rev, &cpu_freq, &cpu_flags); 
		printk("CPU: Processor revision %u.%u.%u.%u, %u MHz%s%s\n",
		       (cpu_rev >> 24) & 0xff,
		       (cpu_rev >> 16) & 0xff,
		       (cpu_rev >> 8) & 0xff,
		       cpu_rev & 0xff,
		       cpu_freq,
		       (cpu_flags & 1) ? " [recovery]" : "",
		       (cpu_flags & 2) ? " [longrun]" : "");
	}
	if ( max >= 0x80860002 ) {
		cpuid(0x80860002, &dummy, &cms_rev1, &cms_rev2, &dummy);
		printk("CPU: Code Morphing Software revision %u.%u.%u-%u-%u\n",
		       (cms_rev1 >> 24) & 0xff,
		       (cms_rev1 >> 16) & 0xff,
		       (cms_rev1 >> 8) & 0xff,
		       cms_rev1 & 0xff,
		       cms_rev2);
	}
	if ( max >= 0x80860006 ) {
		cpuid(0x80860003,
		      (void *)&cpu_info[0],
		      (void *)&cpu_info[4],
		      (void *)&cpu_info[8],
		      (void *)&cpu_info[12]);
		cpuid(0x80860004,
		      (void *)&cpu_info[16],
		      (void *)&cpu_info[20],
		      (void *)&cpu_info[24],
		      (void *)&cpu_info[28]);
		cpuid(0x80860005,
		      (void *)&cpu_info[32],
		      (void *)&cpu_info[36],
		      (void *)&cpu_info[40],
		      (void *)&cpu_info[44]);
		cpuid(0x80860006,
		      (void *)&cpu_info[48],
		      (void *)&cpu_info[52],
		      (void *)&cpu_info[56],
		      (void *)&cpu_info[60]);
		cpu_info[64] = '\0';
		printk("CPU: %s\n", cpu_info);
	}

	/* Unhide possibly hidden flags */
	rdmsr(0x80860004, cap_mask, uk);
	wrmsr(0x80860004, ~0, uk);
	cpuid(0x00000001, &dummy, &dummy, &dummy, &c->x86_capability);
	wrmsr(0x80860004, cap_mask, uk);
}


__initfunc(void get_cpu_vendor(struct cpuinfo_x86 *c))
{
	char *v = c->x86_vendor_id;

	if (!strcmp(v, "GenuineIntel"))
		c->x86_vendor = X86_VENDOR_INTEL;
	else if (!strcmp(v, "AuthenticAMD"))
		c->x86_vendor = X86_VENDOR_AMD;
	else if (!strcmp(v, "CyrixInstead"))
		c->x86_vendor = X86_VENDOR_CYRIX;
	else if (!strcmp(v, "UMC UMC UMC "))
		c->x86_vendor = X86_VENDOR_UMC;
	else if (!strcmp(v, "CentaurHauls"))
		c->x86_vendor = X86_VENDOR_CENTAUR;
	else if (!strcmp(v, "NexGenDriven"))
		c->x86_vendor = X86_VENDOR_NEXGEN;
	else if (!strcmp(v, "RiseRiseRise"))
		c->x86_vendor = X86_VENDOR_RISE;
	else if (!strcmp(v, "GenuineTMx86"))
		c->x86_vendor = X86_VENDOR_TRANSMETA;
	else
		c->x86_vendor = X86_VENDOR_UNKNOWN;
}

struct cpu_model_info {
	int vendor;
	int x86;
	char *model_names[16];
};

static struct cpu_model_info cpu_models[] __initdata = {
	{ X86_VENDOR_INTEL,	4,
	  { "486 DX-25/33", "486 DX-50", "486 SX", "486 DX/2", "486 SL", 
	    "486 SX/2", NULL, "486 DX/2-WB", "486 DX/4", "486 DX/4-WB", NULL, 
	    NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_INTEL,	5,
	  { "Pentium 60/66 A-step", "Pentium 60/66", "Pentium 75 - 200",
	    "OverDrive PODP5V83", "Pentium MMX", NULL, NULL,
	    "Mobile Pentium 75 - 200", "Mobile Pentium MMX", NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_INTEL,	6,
	  { "Pentium Pro A-step", "Pentium Pro", NULL, "Pentium II (Klamath)", 
	    NULL, "Pentium II (Deschutes)", "Mobile Pentium II", 
	    "Pentium III (Katmai)", "Pentium III (Coppermine)", NULL,
	    "Pentium III (Cascades)", NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_AMD,	4,
	  { NULL, NULL, NULL, "486 DX/2", NULL, NULL, NULL, "486 DX/2-WB",
	    "486 DX/4", "486 DX/4-WB", NULL, NULL, NULL, NULL, "Am5x86-WT",
	    "Am5x86-WB" }},
	{ X86_VENDOR_AMD,	5,
	  { "K5/SSA5", "K5",
	    "K5", "K5", NULL, NULL,
	    "K6", "K6", "K6-2",
	    "K6-3", NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_AMD,	6,
	  { "Athlon", "Athlon",
	    NULL, NULL, NULL, NULL,
	    NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_UMC,	4,
	  { NULL, "U5D", "U5S", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_CENTAUR,	5,
	  { NULL, NULL, NULL, NULL, "C6", NULL, NULL, NULL, "C6-2", NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL }},
	{ X86_VENDOR_NEXGEN,	5,
	  { "Nx586", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
};

__initfunc(void identify_cpu(struct cpuinfo_x86 *c))
{
	int i;
	char *p = NULL;
	extern void mcheck_init(void);
	
	c->loops_per_jiffy = loops_per_jiffy;
	c->x86_cache_size = -1;

	get_cpu_vendor(c);

	if (c->x86_vendor == X86_VENDOR_UNKNOWN &&
	    c->cpuid_level < 0)
		return;

	/* It should be possible for the user to override this. */
	if(c->cpuid_level > 0 && 
	   (c->x86_vendor == X86_VENDOR_INTEL || c->x86_vendor == X86_VENDOR_TRANSMETA) &&
	   c->x86_capability&(1<<18)) {
		/* Disable processor serial number */
		unsigned long lo,hi;
		rdmsr(0x119,lo,hi);
		lo |= 0x200000;
		wrmsr(0x119,lo,hi);
		printk(KERN_INFO "CPU serial number disabled.\n");
	}

	mcheck_init();
	
	if (c->x86_vendor == X86_VENDOR_CYRIX) {
		cyrix_model(c);
		return;
	}

	if (c->x86_vendor == X86_VENDOR_AMD && amd_model(c))
		return;
		
	if (c->x86_vendor == X86_VENDOR_TRANSMETA) {
		transmeta_model(c);
		return;
	}
	
	if(c->x86_vendor == X86_VENDOR_CENTAUR && c->x86==6)
	{
		/* The Cyrix III supports model naming and cache queries */
		get_model_name(c);
		display_cacheinfo(c);
		return;
	}

	if (c->cpuid_level > 1) {
		/* supports eax=2  call */
		int regs[4];
		int l1c=0, l1d=0, l2=0, l3=0;	/* Cache sizes */

		cpuid(2, &regs[0], &regs[1], &regs[2], &regs[3]);
		/* Least significant byte of eax says how many times
		 * to call cpuid with value 2 to get cache and TLB
		 * info.
		 */
		if ((regs[0] & 0xFF) != 1 )
			printk(KERN_WARNING "Multiple cache reports are not supported yet\n");

		c->x86_cache_size = 0;

		for ( i = 0 ; i < 4 ; i++ ) 
		{
			int j;

	                 if ( regs[i] < 0 )
	                         continue; /* no useful data */

	                 /* look at all the bytes returned */

	                 for ( j = ( i == 0 ? 8:0 ) ; j < 25 ; j+=8 )
	                 {
				unsigned char rh = regs[i]>>j;
				unsigned char rl;
				
				rl = rh & 0x0F;
				rh >>=4;
				
				switch(rh)
				{
					case 2:
						if(rl)
						{
							printk("%dK L3 cache\n", (rl-1)*512);
							l3 += (rl-1)*512;
						}
						break;
					case 4:
					case 8:
						if(rl)
						{
							printk("%dK L2 cache (%d way)\n",128<<(rl-1), rh);
							l2 += 128<<(rl-1);
						}
						break;
					
					/*
					 *	L1 caches do not count for SMP switching weights,
					 *	they are shadowed by L2.
					 */
					 
					case 6:
		                 		if(rh==6 && rl > 5)
		                 		{
							printk("%dK L1 data cache\n", 8<<(rl - 6));
							l1d+=8<<(rl-6);
						}
						break;
					case 7:
		                 		printk("%dK L1 instruction cache\n",
		                 			rl?(16<<(rl-1)):12);
		                 		l1c+=rl?(16<<(rl-1)):12;
		                 		break;
				}	              
			}   			
		}
		if(l1c && l1d)
			printk("CPU: L1 I Cache: %dK  L1 D Cache: %dK\n",
				l1c, l1d);
		if(l2)
			printk("CPU: L2 Cache: %dK\n", l2);
		if(l3)
			printk("CPU: L3 Cache: %dK\n", l3);
			
		/*
		 *	Assuming L3 is shared. The L1 cache is shadowed by L2
		 *	so doesn't need to be included.
		 */
		 
		c->x86_cache_size += l2;
	}
	
	/*
	 *	Intel finally adopted the AMD/Cyrix extended id naming
	 *	stuff for the 'Pentium IV'
	 */
	 
	if(c->x86_vendor ==X86_VENDOR_INTEL && c->x86 == 15)
	{
		intel_model(c);
		return;
	}
	
	for (i = 0; i < sizeof(cpu_models)/sizeof(struct cpu_model_info); i++) {
		if (cpu_models[i].vendor == c->x86_vendor &&
		    cpu_models[i].x86 == c->x86) {
			if (c->x86_model <= 16)
				p = cpu_models[i].model_names[c->x86_model];

			/* Names for the Pentium II Celeron processors 
                           detectable only by also checking the cache size */
			if ((cpu_models[i].vendor == X86_VENDOR_INTEL)
			    && (cpu_models[i].x86 == 6)){ 
				if(c->x86_model == 6 && c->x86_cache_size == 128) {
					p = "Celeron (Mendocino)"; 
				} else { 
					if (c->x86_model == 5 && c->x86_cache_size == 0) {
						p = "Celeron (Covington)";
					}
				}
			}
		}
	}

	if (p) {
		strcpy(c->x86_model_id, p);
		return;
	}

	sprintf(c->x86_model_id, "%02x/%02x", c->x86_vendor, c->x86_model);
}

/*
 *	Perform early boot up checks for a valid TSC. See arch/i386/kernel/time.c
 */
 
__initfunc(void dodgy_tsc(void))
{
	get_cpu_vendor(&boot_cpu_data);
	
	if(boot_cpu_data.x86_vendor != X86_VENDOR_CYRIX)
	{
		return;
	}
	cyrix_model(&boot_cpu_data);
}
	
	

static char *cpu_vendor_names[] __initdata = {
	"Intel", "Cyrix", "AMD", "UMC", "NexGen", "Centaur", "Rise", "Transmeta" };


__initfunc(void setup_centaur(struct cpuinfo_x86 *c))
{
	u32 hv,lv;
	
	/* Centaur C6 Series */
	if(c->x86==5)
	{
		rdmsr(0x107, lv, hv);
		printk("Centaur FSR was 0x%X ",lv);
		lv|=(1<<1 | 1<<2 | 1<<7);
		/* lv|=(1<<6);	- may help too if the board can cope */
		printk("now 0x%X\n", lv);
		wrmsr(0x107, lv, hv);
		/* Emulate MTRRs using Centaur's MCR. */
		c->x86_capability |= X86_FEATURE_MTRR;

		/* Disable TSC on C6 as per errata. */
		if (c->x86_model ==4) {
			printk ("Disabling bugged TSC.\n");
			c->x86_capability &= ~X86_FEATURE_TSC;
		}

		/* Set 3DNow! on Winchip 2 and above. */
		if (c->x86_model >=8)
		    c->x86_capability |= X86_FEATURE_AMD3D;

		c->x86_capability |=X86_FEATURE_CX8;
	}
	/* Cyrix III 'Samuel' CPU */
	if(c->x86 == 6 && c->x86_model == 6)
	{
		rdmsr(0x1107, lv, hv);
		lv|=(1<<1);	/* Report CX8 */
		lv|=(1<<7);	/* PGE enable */
		wrmsr(0x1107, lv, hv);
		/* Cyrix III */
		c->x86_capability |= X86_FEATURE_CX8;
		rdmsr(0x80000001, lv, hv);
		if(hv&(1<<31))
			c->x86_capability |= X86_FEATURE_AMD3D;
	}	
}

__initfunc(void print_cpu_info(struct cpuinfo_x86 *c))
{
	char *vendor = NULL;

	if (c->x86_vendor < sizeof(cpu_vendor_names)/sizeof(char *))
		vendor = cpu_vendor_names[c->x86_vendor];
	else if (c->cpuid_level >= 0)
		vendor = c->x86_vendor_id;

	if (vendor && strncmp(c->x86_model_id, vendor, strlen(vendor)))
		printk("%s ", vendor);

	if (!c->x86_model_id[0])
		printk("%d86", c->x86);
	else
		printk("%s", c->x86_model_id);

	if (c->x86_mask || c->cpuid_level>=0) 
		printk(" stepping %02x\n", c->x86_mask);
	else
		printk("\n");

	if(c->x86_vendor == X86_VENDOR_CENTAUR) {
		setup_centaur(c);
	}
}

/*
 *	Get CPU information for use by the procfs.
 */

int get_cpuinfo(char * buffer)
{
	char *p = buffer;
	int sep_bug;
	static char *x86_cap_flags[] = {
	        "fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce",
	        "cx8", "apic", "10", "sep", "mtrr", "pge", "mca", "cmov",
	        "16", "pse36", "psn", "19", "20", "21", "22", "mmx",
	        "24", "xmm", "26", "27", "28", "29", "30", "31"
	};
	struct cpuinfo_x86 *c = cpu_data;
	int i, n;

	for(n=0; n<NR_CPUS; n++, c++) {
#ifdef CONFIG_SMP
		if (!(cpu_online_map & (1<<n)))
			continue;
#endif
		p += sprintf(p,"processor\t: %d\n"
			       "vendor_id\t: %s\n"
			       "cpu family\t: %d\n"
			       "model\t\t: %d\n"
			       "model name\t: %s\n",
			       n,
			       c->x86_vendor_id[0] ? c->x86_vendor_id : "unknown",
			       c->x86,
			       c->x86_model,
			       c->x86_model_id[0] ? c->x86_model_id : "unknown");

		if (c->x86_mask || c->cpuid_level >= 0)
			p += sprintf(p, "stepping\t: %d\n", c->x86_mask);
		else
			p += sprintf(p, "stepping\t: unknown\n");

		if (c->x86_capability & X86_FEATURE_TSC) {
			p += sprintf(p, "cpu MHz\t\t: %lu.%03lu\n",
				cpu_khz / 1000, cpu_khz % 1000);
		}

		/* Cache size */
		if (c->x86_cache_size >= 0)
			p += sprintf(p, "cache size\t: %d KB\n", c->x86_cache_size);
		
		/* Modify the capabilities according to chip type */
		switch (c->x86_vendor) {

		    case X86_VENDOR_CYRIX:
			x86_cap_flags[24] = "cxmmx";
			break;

		    case X86_VENDOR_AMD:
			if (c->x86 == 5 && c->x86_model == 6)
				x86_cap_flags[10] = "sep";
			if (c->x86 < 6)
				x86_cap_flags[16] = "fcmov";
			else
				x86_cap_flags[16] = "pat";
			x86_cap_flags[22] = "mmxext";
			x86_cap_flags[24] = "fxsr";
			x86_cap_flags[30] = "3dnowext";
			x86_cap_flags[31] = "3dnow";
			break;
																																										
		    case X86_VENDOR_INTEL:
			x86_cap_flags[16] = "pat";
			x86_cap_flags[19] = "cflush";
			x86_cap_flags[21] = "dtrace";
			x86_cap_flags[22] = "acpi";
			x86_cap_flags[24] = "fxsr";
			x86_cap_flags[26] = "xmm2";
			x86_cap_flags[27] = "ssnp";
			x86_cap_flags[29] = "acc";
			break;

		    case X86_VENDOR_CENTAUR:
			if (c->x86_model >=8)	/* Only Winchip2 and above */
				x86_cap_flags[31] = "3dnow";
			break;

		    default:
		        /* Unknown CPU manufacturer or no specal action needed */
		        break;
		}

		sep_bug = c->x86_vendor == X86_VENDOR_INTEL &&
			  c->x86 == 0x06 &&
			  c->cpuid_level >= 0 &&
			  (c->x86_capability & X86_FEATURE_SEP) &&
			  c->x86_model < 3 &&
			  c->x86_mask < 3;
	
		p += sprintf(p, "fdiv_bug\t: %s\n"
			        "hlt_bug\t\t: %s\n"
			        "sep_bug\t\t: %s\n"
			        "f00f_bug\t: %s\n"
			        "coma_bug\t: %s\n"
			        "fpu\t\t: %s\n"
			        "fpu_exception\t: %s\n"
			        "cpuid level\t: %d\n"
			        "wp\t\t: %s\n"
			        "flags\t\t:",
			     c->fdiv_bug ? "yes" : "no",
			     c->hlt_works_ok ? "no" : "yes",
			     sep_bug ? "yes" : "no",
			     c->f00f_bug ? "yes" : "no",
			     c->coma_bug ? "yes" : "no",
			     c->hard_math ? "yes" : "no",
			     (c->hard_math && ignore_irq13) ? "yes" : "no",
			     c->cpuid_level,
			     c->wp_works_ok ? "yes" : "no");

		for ( i = 0 ; i < 32 ; i++ )
			if ( c->x86_capability & (1 << i) )
				p += sprintf(p, " %s", x86_cap_flags[i]);
		p += sprintf(p, "\nbogomips\t: %lu.%02lu\n\n",
			     c->loops_per_jiffy/(500000/HZ),
			     (c->loops_per_jiffy/(5000/HZ)) % 100);
	}
	return p - buffer;
}
