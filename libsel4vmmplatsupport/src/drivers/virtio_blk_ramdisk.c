/*
 * Simple ramdisk implementation to test virtIO-blk
 * Copyright (c) 2022 UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE   512

/*
 * Provide a 256Mb RAM disk
 */
#define DISK_SIZE (256*(1024*1024))/SECTOR_SIZE)

#define VIRTIO_BLK_READ 0
#define VIRTIO_BLK_WRITE 1
#define VIRTIO_BLK_FLUSH 4
#define VIRTIO_BLK_DISCARD 11
#define VIRTIO_BLK_ZEROES 13

uint8_t disc[SECTOR_SIZE*DISK_SIZE];

static struct virtio_blk_config disk_config  = {
    .capacity = DISK_SIZE,
    .blk_size = 4096,
    .physical_block_exponent = 3, // 4k blocks = 2^3 sectors
    .min_io_size = 1,
    .opt_io_size = 1
};

struct virtblk_req {
	struct virtio_blk_outhdr out_hdr;
	uint8_t status;
	struct sg_table sg_table;
	struct scatterlist sg[];
};

struct virtio_blk_outhdr {
    uint32_t type; // 0: Read; 1: Write; 4: Flush; 11: Discard; 13: Write zeroes
    uint32_t ioprio;
    uint64_t sector;
}

// TODO handle scattered lists.
static int read(void *buf, uint32_t offset, size_t nsec)
{
    if (offset + nsec > DISK_SIZE)
        nsec = DISK_SIZE - offset;
    memcpy(buf, disc + offset * SECTOR_SIZE, nsec * SECTOR_SIZE);
    return nsec;
}

// TODO handle scattered lists.
static int write(void *buf, uint32_t offset, size_t nsec)
{
    if (offset + nsec > DISK_SIZE)
        nsec = DISK_SIZE - offset;
    memcpy(disc + offset * SECTOR_SIZE, buf, nsec * SECTOR_SIZE);
    return nsec;
}

/*
 * Handle reads from PCI BAR 0
 */
bool blk_device_emul_io_in(struct virtio_emul *emul,
                           unsigned int offset,
                           unsigned int size,
                           unsigned int *result)
{
    bool handled = false;
    switch (offset) {
    case VIRTIO_PCI_HOST_FEATURES:
        handled = true;
        assert(size == 4);
        *result = BIT(VIRTIO_BLK_F_BLK_SIZE);  // currently provide a bare-bones LBA only device
        break;

    case VIRTIO_PCI_ISR_STATUS:
        break;

    case 0x14 ... 0x28:
        *result = *(unsigned int *)((char *)&disk_config + port);
        handled = true;
        break;
    }
    return handled;
}

bool blk_device_emul_io_out(struct virtio_emul *emul,
                            unsigned int offset,
                            unsigned int size,
                            unsigned int value)
{
    bool handled = false;
    switch (offset) {
    case VIRTIO_PCI_GUEST_FEATURES:
        handled = true;
        break;
    }
    
    return handled;
}

static int virtblk_process_req(struct virtblk_req *vbr)
{
    // Get the header and work out what we're supposed to do. 
    struct virtio_blk_outhdr *header = vbr->out_hdr;

    uint64_t offset = header->sector;
    uint32_t type = header->type;

    struct sg_table *sg_table = vbr->sg_table;
    struct scatterlist *sgl = sg_table->sgl;

    struct sg_page_iter piter;

    __sg_page_iter_start(&piter, sgl, sg_table->nents, /* idk what this is*/);

    do {
        piter->sg // holds struct scatterlist pointing to the next page

        // send the request and either read or write
    } while (__sg_page_iter_next(&piter));

}

static void
emul_notify_tx(virto_emul_t *emul)
{
    blk_internal_t *blk = (blk_internal_t *)emul->internal;
    struct vring *vring = &emul->virtq.vring[TX_QUEUE];
    /* read the index */
    uint16_t guest_idx = ring_avail_idx(emul, vring);
    /* process what we can of the ring */
    uint16_t idx = emul->virtq.last_idx[TX_QUEUE];
    while (idx != guest_idx) {
        uint16_t desc_head;
        /* read the head of the descriptor chain */
        desc_head = ring_avail(emul, vring, idx);
        struct vring_desc desc;
        uint16_t desc_idx = desc_head;
        do {
            desc = ring_desc(emul, vring, desc_idx);
            //TODO --- Grab SG lists and do them
            struct virtblk_req req;
            vm_guest_read_mem(emul->vm, &req, (uintptr_t)desc.addr, desc.len);

            virblk_process_req(&req);

            desc_idx = desc.next;
        } while (desc.flags & VRING_DESC_F_NEXT); 
    }

    /*...*/

}