#ifndef __ASM_MIPS64_SCATTERLIST_H
#define __ASM_MIPS64_SCATTERLIST_H

struct scatterlist {
	char *  address;	/* Location data is to be transferred to */
	struct page *page;
	unsigned int length;

	__u32 dma_address;
};

struct mmu_sglist {
        char *addr;
        char *__dont_touch;
        unsigned int len;
        __u32 dma_addr;
};

#define ISA_DMA_THRESHOLD (0x00ffffffUL)

#endif /* __ASM_MIPS64_SCATTERLIST_H */
