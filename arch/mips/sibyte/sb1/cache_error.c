/*
 * Copyright (C) 2001 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/sched.h>
#include <asm/mipsregs.h>

/* SB1 definitions */

/* XXX should come from config1 XXX */
#define SB1_CACHE_INDEX_MASK   0x1fe0

#define CP0_ERRCTL_RECOVERABLE (1 << 31)
#define CP0_ERRCTL_DCACHE      (1 << 30)
#define CP0_ERRCTL_ICACHE      (1 << 29)
#define CP0_ERRCTL_MULTIBUS    (1 << 23)
#define CP0_ERRCTL_MC_TLB      (1 << 15)
#define CP0_ERRCTL_MC_TIMEOUT  (1 << 14)

#define CP0_CERRI_TAG_PARITY   (1 << 29)
#define CP0_CERRI_DATA_PARITY  (1 << 28)
#define CP0_CERRI_EXTERNAL     (1 << 26)

#define CP0_CERRI_IDX_VALID(c) (!((c) & CP0_CERRI_EXTERNAL))
#define CP0_CERRI_DATA         (CP0_CERRI_DATA_PARITY)

#define CP0_CERRD_MULTIPLE     (1 << 31)
#define CP0_CERRD_TAG_STATE    (1 << 30)
#define CP0_CERRD_TAG_ADDRESS  (1 << 29)
#define CP0_CERRD_DATA_SBE     (1 << 28)
#define CP0_CERRD_DATA_DBE     (1 << 27)
#define CP0_CERRD_EXTERNAL     (1 << 26)
#define CP0_CERRD_LOAD         (1 << 25)
#define CP0_CERRD_STORE        (1 << 24)
#define CP0_CERRD_FILLWB       (1 << 23)
#define CP0_CERRD_COHERENCY    (1 << 22)
#define CP0_CERRD_DUPTAG       (1 << 21)

#define CP0_CERRD_DPA_VALID(c) (!((c) & CP0_CERRD_EXTERNAL))
#define CP0_CERRD_IDX_VALID(c) \
   (((c) & (CP0_CERRD_LOAD | CP0_CERRD_STORE)) ? (!((c) & CP0_CERRD_EXTERNAL)) : 0)
#define CP0_CERRD_CAUSES \
   (CP0_CERRD_LOAD | CP0_CERRD_STORE | CP0_CERRD_FILLWB | CP0_CERRD_COHERENCY | CP0_CERRD_DUPTAG)
#define CP0_CERRD_TYPES \
   (CP0_CERRD_TAG_STATE | CP0_CERRD_TAG_ADDRESS | CP0_CERRD_DATA_SBE | CP0_CERRD_DATA_DBE | CP0_CERRD_EXTERNAL)
#define CP0_CERRD_DATA         (CP0_CERRD_DATA_SBE | CP0_CERRD_DATA_DBE)

static uint32_t	extract_ic(unsigned short addr, int data);
static uint32_t	extract_dc(unsigned short addr, int data);

spinlock_t in_cacheerr = SPIN_LOCK_UNLOCKED;

static inline void breakout_errctl(unsigned int val)
{
	if (val & CP0_ERRCTL_RECOVERABLE)
		printk(" recoverable");
	if (val & CP0_ERRCTL_DCACHE)
		printk(" dcache");
	if (val & CP0_ERRCTL_ICACHE)
		printk(" icache");
	if (val & CP0_ERRCTL_MULTIBUS)
		printk(" multiple-buserr");
	printk("\n");
}

static inline void breakout_cerri(unsigned int val)
{
	if (val & CP0_CERRI_TAG_PARITY)
		printk(" tag-parity");
	if (val & CP0_CERRI_DATA_PARITY)
		printk(" data-parity");
	if (val & CP0_CERRI_EXTERNAL)
		printk(" external");
	printk("\n");
}

static inline void breakout_cerrd(unsigned int val)
{
	switch (val & CP0_CERRD_CAUSES) {
	case CP0_CERRD_LOAD:
		printk(" load,");
		break;
	case CP0_CERRD_STORE:
		printk(" store,");
		break;
	case CP0_CERRD_FILLWB:
		printk(" fill/wb,");
		break;
	case CP0_CERRD_COHERENCY:
		printk(" coherency,");
		break;
	case CP0_CERRD_DUPTAG:
		printk(" duptags,");
		break;
	default:
		printk(" NO CAUSE,");
		break;
	}
	if (!(val & CP0_CERRD_TYPES))
		printk(" NO TYPE");
	else {
		if (val & CP0_CERRD_MULTIPLE)
			printk(" multi-err");
		if (val & CP0_CERRD_TAG_STATE)
			printk(" tag-state");
		if (val & CP0_CERRD_TAG_ADDRESS)
			printk(" tag-address");
		if (val & CP0_CERRD_DATA_SBE)
			printk(" data-SBE");
		if (val & CP0_CERRD_DATA_DBE)
			printk(" data-DBE");
		if (val & CP0_CERRD_EXTERNAL)
			printk(" external");
	}
	printk("\n");
}

asmlinkage void sb1_cache_error(void)
{
	uint64_t cerr_dpa;
	uint32_t errctl, cerr_i, cerr_d, dpalo, dpahi, eepc, res;

	/* Prevent re-entrance in the SMP case */
	spin_lock(&in_cacheerr);
	printk("Cache error exception on CPU %x:\n",
	       (read_32bit_cp0_register(CP0_PRID) >> 25) & 0x7);
	__asm__ __volatile__ (
		".set push\n\t"
		".set mips64\n\t"
		".set noat\n\t"
		"mfc0   %0, $26, 0\n\t"
		"mfc0   %1, $27, 0\n\t"
		"mfc0   %2, $27, 1\n\t"
		"dmfc0  $1, $27, 3\n\t"
		"dsrl32 %3, $1, 0 \n\t"
		"sll    %4, $1, 0 \n\t"
		"mfc0   %5, $30\n\t"
		".set pop\n"
		: "=r" (errctl), "=r" (cerr_i), "=r" (cerr_d),
		  "=r" (dpahi), "=r" (dpalo), "=r" (eepc));
	cerr_dpa = (((uint64_t)dpahi) << 32) | dpalo;
	printk(" cp0_errorepc ==   %08x\n", eepc);
	printk(" cp0_errctl   ==   %08x", errctl);
	breakout_errctl(errctl);
	if (errctl & CP0_ERRCTL_ICACHE) {
		printk(" cp0_cerr_i   ==   %08x", cerr_i);
		breakout_cerri(cerr_i);
		if (CP0_CERRI_IDX_VALID(cerr_i)) {
			if ((eepc & SB1_CACHE_INDEX_MASK) != (cerr_i & SB1_CACHE_INDEX_MASK))
				printk(" cerr_i idx doesn't match eepc\n");
			else {
				res = extract_ic(cerr_i & SB1_CACHE_INDEX_MASK,
						 (cerr_i & CP0_CERRI_DATA) != 0);
				if (!(res & cerr_i))
					printk("...didn't see indicated icache problem\n");
			}
		}
	}
	if (errctl & CP0_ERRCTL_DCACHE) {
		printk(" cp0_cerr_d   ==   %08x", cerr_d);
		breakout_cerrd(cerr_d);
		if (CP0_CERRD_DPA_VALID(cerr_d)) {
			printk(" cp0_cerr_dpa == %010llx\n", cerr_dpa);
			if (!CP0_CERRD_IDX_VALID(cerr_d)) {
				res = extract_dc(cerr_dpa & SB1_CACHE_INDEX_MASK,
						 (cerr_d & CP0_CERRD_DATA) != 0);
				if (!(res & cerr_d))
					printk("...didn't see indicated dcache problem\n");
			} else {
				if ((cerr_dpa & SB1_CACHE_INDEX_MASK) != (cerr_d & SB1_CACHE_INDEX_MASK))
					printk(" cerr_d idx doesn't match cerr_dpa\n");
				else {
					res = extract_dc(cerr_d & SB1_CACHE_INDEX_MASK,
							 (cerr_d & CP0_CERRD_DATA) != 0);
					if (!(res & cerr_d))
						printk("...didn't see indicated problem\n");
				}
			}
		}
	}

	while (1);
	/*
	 * This tends to make things get really ugly; let's just stall instead.
	 *    panic("Can't handle the cache error!");
	 */
}


/* Parity lookup table. */
static const uint8_t parity[256] = {
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
	1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0
};

/* Masks to select bits for Hamming parity, mask_72_64[i] for bit[i] */
static const uint64_t mask_72_64[8] = {
	0x0738C808099264FFL,
	0x38C808099264FF07L,
	0xC808099264FF0738L,
	0x08099264FF0738C8L,
	0x099264FF0738C808L,
	0x9264FF0738C80809L,
	0x64FF0738C8080992L,
	0xFF0738C808099264L
};

/* Calculate the parity on a range of bits */
static char range_parity(uint64_t dword, int max, int min)
{
	char parity = 0;
	int i;
	dword >>= min;
	for (i=max-min; i>=0; i--) {
		if (dword & 0x1)
			parity = !parity;
		dword >>= 1;
	}
	return parity;
}

/* Calculate the 4-bit even byte-parity for an instruction */
static unsigned char inst_parity(uint32_t word)
{
	int i, j;
	char parity = 0;
	for (j=0; j<4; j++) {
		char byte_parity = 0;
		for (i=0; i<8; i++) {
			if (word & 0x80000000)
				byte_parity = !byte_parity;
			word <<= 1;
		}
		parity <<= 1;
		parity |= byte_parity;
	}
	return parity;
}

static uint32_t extract_ic(unsigned short addr, int data)
{
	unsigned short way;
	int valid;
	uint64_t taglo, va, tlo_tmp;
	uint32_t taghi, taglolo, taglohi;
	uint8_t lru;
	int res = 0;

	printk("Icache index 0x%04x  ", addr);
	for (way = 0; way < 4; way++) {
		/* Index-load-tag-I */
		__asm__ __volatile__ (
			".set push        \n\t"
			".set noreorder   \n\t"
			".set mips64      \n\t"
			".set noat        \n\t"
			"cache  4, 0(%3)  \n\t"
			"mfc0   %0, $29, 0\n\t"
			"dmfc0  $1, $28, 0\n\t"
			"dsrl32 %1, $1, 0 \n\t"
			"sll    %2, $1, 0 \n\t"
			".set pop         \n"
			: "=r" (taghi), "=r" (taglohi),
			  "=r" (taglolo)
			: "r" ((way << 13) | addr));
		taglo = ((unsigned long long)taglohi << 32) | taglolo;
		if (way == 0) {
			lru = (taghi >> 14) & 0xff;
			printk("[Bank %d Set 0x%02x]  LRU > %d %d %d %d > MRU\n",
			       ((addr >> 5) & 0x3), /* bank */
			       ((addr >> 7) & 0x3f), /* index */
			       (lru & 0x3),
			       ((lru >> 2) & 0x3),
			       ((lru >> 4) & 0x3),
			       ((lru >> 6) & 0x3));
		}
		va = (taglo & 0xC0000FFFFFFFE000) | addr;
		if ((taglo & (1 << 31)) && (((taglo >> 62) & 0x3) == 3))
			va |= 0x3FFFF00000000000;
		valid = ((taghi >> 29) & 1);
		if (valid) {
			tlo_tmp = taglo & 0xfff3ff;
			if (((taglo >> 10) & 1) ^ range_parity(tlo_tmp, 23, 0)) {
				printk("   ** bad parity in VTag0/G/ASID\n");
				res |= CP0_CERRI_TAG_PARITY;
			}
			if (((taglo >> 11) & 1) ^ range_parity(taglo, 63, 24)) {
				printk("   ** bad parity in R/VTag1\n");
				res |= CP0_CERRI_TAG_PARITY;
			}
		}
		if (valid ^ ((taghi >> 27) & 1)) {
			printk("   ** bad parity for valid bit\n");
			res |= CP0_CERRI_TAG_PARITY;
		}
		printk(" %d  [VA %016llx]  [Vld? %d]  raw tags: %08X-%016llX\n",
		       way, va, valid, taghi, taglo);

		if (data) {
			uint32_t datahi, insta, instb;
			uint8_t predecode;
			int offset;

			/* (hit all banks and ways) */
			for (offset = 0; offset < 4; offset++) {
				/* Index-load-data-I */
				__asm__ __volatile__ (
					".set push        \n\t"
					".set noreorder   \n\t"
					".set mips64      \n\t"
					".set noat        \n\t"
					"cache  6, 0(%3)  \n\t"
					"mfc0   %0, $29, 1\n\t"
					"dmfc0  $1, $28, 1\n\t"
					"dsrl32 %1, $1, 0 \n\t"
					"sll    %2, $1, 0 \n\t"
					".set pop         \n"
					: "=r" (datahi), "=r" (insta),
					  "=r" (instb)
					: "r" ((way << 13) | addr | (offset << 3)));
				predecode = (datahi >> 8) & 0xff;
				if (((datahi >> 16) & 1) != (uint32_t)range_parity(predecode, 7, 0)) {
					printk("   ** bad parity in predecode\n");
					res |= CP0_CERRI_DATA_PARITY;
				}
				/* XXXKW should/could check predecode bits themselves */
				if (((datahi >> 4) & 0xf) ^ inst_parity(insta)) {
					printk("   ** bad parity in instruction a\n");
					res |= CP0_CERRI_DATA_PARITY;
				}
				if ((datahi & 0xf) ^ inst_parity(instb)) {
					printk("   ** bad parity in instruction b\n");
					res |= CP0_CERRI_DATA_PARITY;
				}
				printk("  %05X-%08X%08X", datahi, insta, instb);
			}
			printk("\n");
		}
	}
	return res;
}

/* Compute the ECC for a data doubleword */
static uint8_t dc_ecc(uint64_t dword)
{
	uint64_t t;
	uint32_t w;
	uint8_t  p;
	int      i;

	p = 0;
	for (i = 7; i >= 0; i--)
	{
		p <<= 1;
		t = dword & mask_72_64[i];
		w = (uint32_t)(t >> 32);
		p ^= (parity[w>>24] ^ parity[(w>>16) & 0xFF]
		      ^ parity[(w>>8) & 0xFF] ^ parity[w & 0xFF]);
		w = (uint32_t)(t & 0xFFFFFFFF);
		p ^= (parity[w>>24] ^ parity[(w>>16) & 0xFF]
		      ^ parity[(w>>8) & 0xFF] ^ parity[w & 0xFF]);
	}
	return p;
}

struct dc_state {
	unsigned char val;
	char *name;
};

static struct dc_state dc_states[] = {
	{ 0x00, "INVALID" },
	{ 0x0f, "COH-SHD" },
	{ 0x13, "NCO-E-C" },
	{ 0x19, "NCO-E-D" },
	{ 0x16, "COH-E-C" },
	{ 0x1c, "COH-E-D" },
	{ 0xff, "*ERROR*" }
};

#define DC_TAG_VALID(state) \
    (((state) == 0xf) || ((state) == 0x13) || ((state) == 0x19) || ((state == 0x16)) || ((state) == 0x1c))

static char *dc_state_str(unsigned char state)
{
	struct dc_state *dsc = dc_states;
	while (dsc->val != 0xff) {
		if (dsc->val == state)
			break;
		dsc++;
	}
	return dsc->name;
}

static uint32_t extract_dc(unsigned short addr, int data)
{
	int valid, way;
	unsigned char state;
	uint64_t taglo, pa;
	uint32_t taghi, taglolo, taglohi;
	uint8_t ecc, lru;
	int res = 0;

	printk("Dcache index 0x%04x  ", addr);
	for (way = 0; way < 4; way++) {
		/* Index-load-tag-D */
		__asm__ __volatile__ (
			".set push        \n\t"
			".set noreorder   \n\t"
			".set mips64      \n\t"
			".set noat        \n\t"
			"cache  5, 0(%3)  \n\t"
			"mfc0   %0, $29, 2\n\t"
			"dmfc0  $1, $28, 2\n\t"
			"dsrl32 %1, $1, 0 \n\t"
			"sll    %2, $1, 0 \n\t"
			".set pop         \n"
			: "=r" (taghi), "=r" (taglohi),
			  "=r" (taglolo)
			: "r" ((way << 13) | addr));

		taglo = ((unsigned long long)taglohi << 32) | taglolo;
		pa = (taglo & 0xFFFFFFE000) | addr;
		if (way == 0) {
			lru = (taghi >> 14) & 0xff;
			printk("[Bank %d Set 0x%02x]  LRU > %d %d %d %d > MRU\n",
			       ((addr >> 11) & 0x2) | ((addr >> 5) & 1), /* bank */
			       ((addr >> 6) & 0x3f), /* index */
			       (lru & 0x3),
			       ((lru >> 2) & 0x3),
			       ((lru >> 4) & 0x3),
			       ((lru >> 6) & 0x3));
		}
		state = (taghi >> 25) & 0x1f;
		valid = DC_TAG_VALID(state);
		printk(" %d  [PA %010llx]  [state %s (%02x)]  raw tags: %08X-%016llX\n",
		       way, pa, dc_state_str(state), state, taghi, taglo);
		if (valid) {
			if (((taglo >> 11) & 1) ^ range_parity(taglo, 39, 26)) {
				printk("   ** bad parity in PTag1\n");
				res |= CP0_CERRD_TAG_ADDRESS;
			}
			if (((taglo >> 10) & 1) ^ range_parity(taglo, 25, 13)) {
				printk("   ** bad parity in PTag0\n");
				res |= CP0_CERRD_TAG_ADDRESS;
			}
		} else {
			res |= CP0_CERRD_TAG_STATE;
		}

		if (data) {
			uint64_t datalo;
			uint32_t datalohi, datalolo, datahi;
			int offset;

			for (offset = 0; offset < 4; offset++) {
				/* Index-load-data-D */
				__asm__ __volatile__ (
					".set push        \n\t"
					".set noreorder   \n\t"
					".set mips64      \n\t"
					".set noat        \n\t"
					"cache  7, 0(%3)  \n\t"
					"mfc0   %0, $29, 3\n\t"
					"dmfc0  $1, $28, 3\n\t"
					"dsrl32 %1, $1, 0 \n\t"
					"sll    %2, $1, 0 \n\t"
					".set pop         \n"
					: "=r" (datahi), "=r" (datalohi),
					"=r" (datalolo)
					: "r" ((way << 13) | addr | (offset << 3)));
				datalo = ((unsigned long long)datalohi << 32) | datalolo;
				ecc = dc_ecc(datalo);
				if (ecc != datahi) {
					int bits = 0;
					printk("  ** bad ECC (%02x %02x) ->",
					       datahi, ecc);
					ecc ^= datahi;
					while (ecc) {
						if (ecc & 1) bits++;
						ecc >>= 1;
					}
					res |= (bits == 1) ? CP0_CERRD_DATA_SBE : CP0_CERRD_DATA_DBE;
				}
				printk("  %02X-%016llX", datahi, datalo);
			}
			printk("\n");
		}
	}
	return res;
}
