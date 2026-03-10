#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "virtio.h"

#define PCIE_ECAM_BASE   0x20000000UL
#define PCI_BUS_MAX      256
#define PCI_DEV_MAX      32
#define PCI_FUNC_MAX     8

#define PCI_VENDOR_QEMU  0x1af4
#define PCI_ID_VIRTIO_BLK_MODERN 0x1042
#define PCI_ID_VIRTIO_BLK_LEGACY 0x1001

#define PCI_STATUS       0x06
#define PCI_HEADER_TYPE  0x0e
#define PCI_CAP_PTR      0x34
#define PCI_BAR0         0x10
#define PCI_SUBCLASS     0x0a
#define PCI_CLASS_CODE   0x0b

#define PCI_STATUS_CAP_LIST 0x10

#define PCI_CAP_ID_VNDR  0x09
#define PCI_CAP_ID_MSI   0x05

#define PCI_BAR_NUM      6
#define PCI_MMIO_ALLOC_BASE 0x40000000UL
#define PCI_MMIO_ALLOC_MAX  0x80000000UL

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG    3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4

struct virtio_pci_common_cfg {
  uint32 device_feature_select;
  uint32 device_feature;
  uint32 driver_feature_select;
  uint32 driver_feature;
  uint16 msix_config;
  uint16 num_queues;
  uint8 device_status;
  uint8 config_generation;

  uint16 queue_select;
  uint16 queue_size;
  uint16 queue_msix_vector;
  uint16 queue_enable;
  uint16 queue_notify_off;
  uint64 queue_desc;
  uint64 queue_driver;
  uint64 queue_device;
} __attribute__((packed));

static struct {
  struct spinlock lock;

  int use_ramdisk;
  int ready;

  volatile struct virtio_pci_common_cfg *common;
  volatile uint8 *notify_base;
  volatile uint8 *isr;
  uint32 notify_off_mul;

  struct virtq_desc *desc;
  volatile struct virtq_avail *avail;
  volatile struct virtq_used *used;

  char free[NUM];
  uint16 used_idx;

  struct {
    char status;
  } info[NUM];

  struct virtio_blk_req ops[NUM];
} disk;

static uint64 pci_mmio_alloc = PCI_MMIO_ALLOC_BASE;

static inline uint64
kva2pa(uint64 kva)
{
  // DMW windows: strip the direct-map window prefix to recover physical addr.
  if((kva & DMWIN0_MASK) == DMWIN0_MASK)
    return kva & ~DMWIN0_MASK;
  if((kva & DMWIN1_MASK) == DMWIN1_MASK)
    return kva & ~DMWIN1_MASK;
  return kva;
}

static inline uint64
mmio_pa2va(uint64 pa)
{
  return pa | DMWIN1_MASK;
}

static inline uint64
pci_cfg_addr(uint bus, uint dev, uint fun, uint off)
{
  return mmio_pa2va(PCIE_ECAM_BASE + ((uint64)bus << 20) + ((uint64)dev << 15) + ((uint64)fun << 12) + off);
}

static inline uint32
pci_read32(uint bus, uint dev, uint fun, uint off)
{
  uint o = off & ~0x3;
  return *(volatile uint32 *)pci_cfg_addr(bus, dev, fun, o);
}

static inline uint16
pci_read16(uint bus, uint dev, uint fun, uint off)
{
  uint32 v = pci_read32(bus, dev, fun, off);
  uint shift = (off & 0x2) * 8;
  return (v >> shift) & 0xffff;
}

static inline uint8
pci_read8(uint bus, uint dev, uint fun, uint off)
{
  uint32 v = pci_read32(bus, dev, fun, off);
  uint shift = (off & 0x3) * 8;
  return (v >> shift) & 0xff;
}



static inline void
pci_write32(uint bus, uint dev, uint fun, uint off, uint32 val)
{
  uint o = off & ~0x3;
  *(volatile uint32 *)pci_cfg_addr(bus, dev, fun, o) = val;
}

static inline void
pci_write16(uint bus, uint dev, uint fun, uint off, uint16 val)
{
  uint32 old = pci_read32(bus, dev, fun, off);
  uint shift = (off & 0x2) * 8;
  uint32 mask = 0xffffU << shift;
  uint32 nv = (old & ~mask) | ((uint32)val << shift);
  pci_write32(bus, dev, fun, off, nv);
}
static uint64
pci_get_bar(uint bus, uint dev, uint fun, int barn)
{
  uint off = PCI_BAR0 + barn * 4;
  uint32 lo = pci_read32(bus, dev, fun, off);

  if(lo & 0x1)
    return 0;

  uint64 base = lo & ~0xfU;
  if(((lo >> 1) & 0x3) == 0x2){
    uint32 hi = pci_read32(bus, dev, fun, off + 4);
    base |= ((uint64)hi << 32);
  }
  return base;
}


static inline int
virtio_blk_pci_devid(uint16 devid)
{
  return devid == PCI_ID_VIRTIO_BLK_MODERN || devid == PCI_ID_VIRTIO_BLK_LEGACY;
}

static int
virtio_pci_find(uint *rb, uint *rd, uint *rf, uint16 *rdevid)
{
  for(uint bus = 0; bus < PCI_BUS_MAX; bus++){
    for(uint dev = 0; dev < PCI_DEV_MAX; dev++){
      uint16 vend = pci_read16(bus, dev, 0, 0x00);
      if(vend == 0xffff)
        continue;

      int funcs = 1;
      uint8 htype = pci_read8(bus, dev, 0, PCI_HEADER_TYPE);
      if(htype & 0x80)
        funcs = PCI_FUNC_MAX;

      for(uint fun = 0; fun < (uint)funcs; fun++){
        vend = pci_read16(bus, dev, fun, 0x00);
        uint16 devid = pci_read16(bus, dev, fun, 0x02);
        if(vend == PCI_VENDOR_QEMU){
          if(virtio_blk_pci_devid(devid)){
            *rb = bus;
            *rd = dev;
            *rf = fun;
            *rdevid = devid;
            return 0;
          }
        }
      }
    }
  }
  return -1;
}

static inline uint64
align_up(uint64 x, uint64 a)
{
  if(a == 0)
    return x;
  return (x + a - 1) & ~(a - 1);
}

static uint64
pci_assign_bar_if_needed(uint bus, uint dev, uint fun, int barn, uint64 hint_len)
{
  uint64 base = pci_get_bar(bus, dev, fun, barn);
  if(base != 0)
    return base;

  uint off = PCI_BAR0 + barn * 4;
  uint32 orig_lo = pci_read32(bus, dev, fun, off);
  if(orig_lo & 0x1)
    return 0;

  int is64 = (((orig_lo >> 1) & 0x3) == 0x2);
  uint32 orig_hi = 0;
  if(is64)
    orig_hi = pci_read32(bus, dev, fun, off + 4);

  pci_write32(bus, dev, fun, off, 0xffffffffU);
  uint32 mask_lo = pci_read32(bus, dev, fun, off);
  uint32 mask_hi = 0xffffffffU;
  if(is64){
    pci_write32(bus, dev, fun, off + 4, 0xffffffffU);
    mask_hi = pci_read32(bus, dev, fun, off + 4);
  }

  uint64 size;
  if(is64){
    uint64 m = ((uint64)mask_hi << 32) | (mask_lo & ~0xfU);
    size = (~m) + 1;
  } else {
    uint32 m = mask_lo & ~0xfU;
    size = ((uint64)(~m)) + 1;
  }

  if(hint_len != 0)
    size = hint_len;

  if(size == 0 || (size & (size - 1)) != 0)
    size = 0x1000;
  if(size > (16UL << 20))
    size = 0x1000;

  uint64 alloc = align_up(pci_mmio_alloc, size);
  if(alloc + size > PCI_MMIO_ALLOC_MAX)
    return 0;
  pci_mmio_alloc = alloc + size;

  uint32 lo = (uint32)(alloc & 0xffffffffUL);
  lo = (lo & ~0xfU) | (orig_lo & 0xfU);
  pci_write32(bus, dev, fun, off, lo);
  if(is64)
    pci_write32(bus, dev, fun, off + 4, (uint32)(alloc >> 32));

  base = pci_get_bar(bus, dev, fun, barn);
  if(base == 0){
    // rollback to original values on failure.
    pci_write32(bus, dev, fun, off, orig_lo);
    if(is64)
      pci_write32(bus, dev, fun, off + 4, orig_hi);
  }
  return base;
}

static int
virtio_pci_parse_caps(uint bus, uint dev, uint fun)
{
  uint16 status = pci_read16(bus, dev, fun, PCI_STATUS);
  uint8 cap = pci_read8(bus, dev, fun, PCI_CAP_PTR) & ~0x3;

  if((status & PCI_STATUS_CAP_LIST) == 0)
    return -1;

  int guard = 0;
  while(cap && guard++ < 64){
    uint8 cap_id = pci_read8(bus, dev, fun, cap + 0);
    uint8 next = pci_read8(bus, dev, fun, cap + 1) & ~0x3;
    uint8 cap_len = pci_read8(bus, dev, fun, cap + 2);

    if(cap_id == PCI_CAP_ID_VNDR && cap_len >= 16){
      uint8 cfg_type = pci_read8(bus, dev, fun, cap + 3);
      uint8 bar = pci_read8(bus, dev, fun, cap + 4);
      uint32 off = pci_read32(bus, dev, fun, cap + 8);
      uint32 len = pci_read32(bus, dev, fun, cap + 12);
      uint64 bar_base = 0;

      if(bar < PCI_BAR_NUM)
        bar_base = pci_assign_bar_if_needed(bus, dev, fun, bar, len);

      if(bar < PCI_BAR_NUM){
        uint64 addr = mmio_pa2va(bar_base + off);
        switch(cfg_type){
        case VIRTIO_PCI_CAP_COMMON_CFG:
          disk.common = (volatile struct virtio_pci_common_cfg *)addr;
          break;
        case VIRTIO_PCI_CAP_NOTIFY_CFG:
          disk.notify_base = (volatile uint8 *)addr;
          if(cap_len >= 20)
            disk.notify_off_mul = pci_read32(bus, dev, fun, cap + 16);
          break;
        case VIRTIO_PCI_CAP_ISR_CFG:
          disk.isr = (volatile uint8 *)addr;
          break;
        case VIRTIO_PCI_CAP_DEVICE_CFG:
          break;
        }
      }
    }

    if(next == cap)
      break;
    cap = next;
  }

  if(disk.common == 0 || disk.notify_base == 0)
    return -1;

  return 0;
}

static inline uint8
v_get_status(void)
{
  return disk.common->device_status;
}

static inline void
v_set_status(uint8 s)
{
  disk.common->device_status = s;
}

static inline uint32
v_get_features(uint32 sel)
{
  disk.common->device_feature_select = sel;
  return disk.common->device_feature;
}

static inline void
v_set_features(uint32 sel, uint32 val)
{
  disk.common->driver_feature_select = sel;
  disk.common->driver_feature = val;
}

static void
v_kick_queue0(void)
{
  uint16 off = disk.common->queue_notify_off;
  uint32 mul = disk.notify_off_mul;
  volatile uint16 *notify = (volatile uint16 *)(disk.notify_base + off * mul);
  *notify = 0;
}

static int
alloc_desc(void)
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

static void
free_desc(int i)
{
  if(i < 0 || i >= NUM)
    panic("free_desc");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
}

static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

static int
alloc3_desc(int idx[3])
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

static int
virtio_pci_disk_init(void)
{
  uint bus, dev, fun;
  uint16 devid = 0;

  if(virtio_pci_find(&bus, &dev, &fun, &devid) < 0){
    printf("disk: no virtio-blk-pci (legacy/modern) found, fallback ramdisk\n");
    return -1;
  }

  uint16 cmd = pci_read16(bus, dev, fun, 0x04);
  if((cmd & 0x6) != 0x6){
    uint16 ncmd = cmd | 0x6;
    pci_write16(bus, dev, fun, 0x04, ncmd);
  }

  if(virtio_pci_parse_caps(bus, dev, fun) < 0){
    printf("disk: virtio pci modern caps not usable (common=%p notify=%p mul=%d), fallback ramdisk\n", disk.common, disk.notify_base, disk.notify_off_mul);
    return -1;
  }

  v_set_status(0);

  uint8 status = 0;
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  v_set_status(status);
  status |= VIRTIO_CONFIG_S_DRIVER;
  v_set_status(status);

  uint32 f0 = v_get_features(0);
  uint32 f1 = v_get_features(1);

  f0 &= ~(1U << VIRTIO_BLK_F_RO);
  f0 &= ~(1U << VIRTIO_BLK_F_SCSI);
  f0 &= ~(1U << VIRTIO_BLK_F_CONFIG_WCE);
  f0 &= ~(1U << VIRTIO_BLK_F_MQ);
  f0 &= ~(1U << VIRTIO_F_ANY_LAYOUT);
  f0 &= ~(1U << VIRTIO_RING_F_EVENT_IDX);
  f0 &= ~(1U << VIRTIO_RING_F_INDIRECT_DESC);

  v_set_features(0, f0);
  v_set_features(1, f1);

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  v_set_status(status);

  if((v_get_status() & VIRTIO_CONFIG_S_FEATURES_OK) == 0){
    printf("disk: FEATURES_OK unset\n");
    return -1;
  }

  disk.common->queue_select = 0;
  if(disk.common->queue_size == 0 || disk.common->queue_size < NUM){
    printf("disk: queue0 unsupported size=%d\n", disk.common->queue_size);
    return -1;
  }

  disk.desc = kalloc();
  disk.avail = kalloc();
  disk.used = kalloc();
  if(!disk.desc || !disk.avail || !disk.used){
    printf("disk: kalloc queue failed\n");
    return -1;
  }

  memset(disk.desc, 0, PGSIZE);
  memset((void*)disk.avail, 0, PGSIZE);
  memset((void*)disk.used, 0, PGSIZE);

  disk.common->queue_select = 0;
  disk.common->queue_size = NUM;
  disk.common->queue_desc = kva2pa((uint64)disk.desc);
  disk.common->queue_driver = kva2pa((uint64)disk.avail);
  disk.common->queue_device = kva2pa((uint64)disk.used);
  disk.common->queue_enable = 1;

  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  disk.used_idx = 0;

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  v_set_status(status);

  if(disk.isr)
    (void)*disk.isr;

  printf("disk: virtio-blk-pci devid=0x%x at %d:%d.%d ready\n", devid, bus, dev, fun);
  return 0;
}

void
disk_init(void)
{
  initlock(&disk.lock, "disk");
  disk.use_ramdisk = 1;
  disk.ready = 0;

  if(virtio_pci_disk_init() == 0){
    disk.use_ramdisk = 0;
    disk.ready = 1;
    return;
  }

  ramdiskinit();
}

void
disk_rw(struct buf *b, int write)
{
  if(disk.use_ramdisk){
    ramdiskrw(b, write);
    return;
  }

  if(!disk.ready)
    panic("disk not ready");

  acquire(&disk.lock);

  int idx[3];
  while(alloc3_desc(idx) < 0)
    ;

  struct virtio_blk_req *req = &disk.ops[idx[0]];
  req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
  req->reserved = 0;
  req->sector = b->blockno * (BSIZE / 512);

  disk.desc[idx[0]].addr = kva2pa((uint64)req);
  disk.desc[idx[0]].len = sizeof(*req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = kva2pa((uint64)b->data);
  disk.desc[idx[1]].len = BSIZE;
  disk.desc[idx[1]].flags = (write ? 0 : VRING_DESC_F_WRITE) | VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff;
  disk.desc[idx[2]].addr = kva2pa((uint64)&disk.info[idx[0]].status);
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[2]].next = 0;

  uint16 avail_idx = disk.avail->idx;
  disk.avail->ring[avail_idx % NUM] = idx[0];
  __sync_synchronize();
  disk.avail->idx = avail_idx + 1;
  __sync_synchronize();

  v_kick_queue0();

  uint64 spin = 0;
  while(disk.used_idx == disk.used->idx){
    spin++;
    if(spin > (1UL<<31)){
      printf("disk: timeout blk=%d desc_pa=%p avail_pa=%p used_pa=%p\n",
             b->blockno,
             (void*)kva2pa((uint64)disk.desc),
             (void*)kva2pa((uint64)disk.avail),
             (void*)kva2pa((uint64)disk.used));
      panic("virtio timeout");
    }
  }

  __sync_synchronize();
  int id = disk.used->ring[disk.used_idx % NUM].id;
  disk.used_idx++;

  if(disk.info[id].status != 0){
    printf("disk: virtio bad status=%d blk=%d id=%d\n", disk.info[id].status, b->blockno, id);
    panic("virtio bad status");
  }

  free_chain(id);

  release(&disk.lock);
}
