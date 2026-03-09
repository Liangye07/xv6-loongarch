//
// virtio-blk driver for LoongArch xv6:
// - preferred path: modern virtio-pci capabilities
// - fallback path: legacy virtio-pci (for device id 0x1001 without vendor caps)
//

#include "types.h"
#include "loongarch.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

#define PCI_VENDOR_ID_OFF     0x00
#define PCI_DEVICE_ID_OFF     0x02
#define PCI_COMMAND_OFF       0x04
#define PCI_STATUS_OFF        0x06
#define PCI_SUBCLASS_OFF      0x0a
#define PCI_CLASS_OFF         0x0b
#define PCI_HEADER_TYPE_OFF   0x0e
#define PCI_CAP_PTR_OFF       0x34
#define PCI_BAR0_OFF          0x10

#define PCI_COMMAND_MEMORY    0x2
#define PCI_COMMAND_MASTER    0x4
#define PCI_COMMAND_IO        0x1

#define PCI_CAP_ID_VENDOR     0x09

#define VIRTIO_PCI_CAP_COMMON_CFG  1
#define VIRTIO_PCI_CAP_NOTIFY_CFG  2
#define VIRTIO_PCI_CAP_ISR_CFG     3

#define VIRTIO_PCI_VENDOR_ID       0x1af4
#define VIRTIO_PCI_DEVICE_BLK      0x1001
#define VIRTIO_PCI_DEVICE_BLK_MOD  0x1042

#define PCI_ECAM_PHYS_BASE   0x20000000UL
#define PCI_ECAM_BASE        (PCI_ECAM_PHYS_BASE | DMWIN1_MASK)

#define KV2PA(va)            ((uint64)(va) & ~DMWIN0_MASK)

// legacy virtio-pci register offsets (transitional interface)
#define VIRTIO_PCI_HOST_FEATURES   0
#define VIRTIO_PCI_GUEST_FEATURES  4
#define VIRTIO_PCI_QUEUE_PFN       8
#define VIRTIO_PCI_QUEUE_NUM       12
#define VIRTIO_PCI_QUEUE_SEL       14
#define VIRTIO_PCI_QUEUE_NOTIFY    16
#define VIRTIO_PCI_STATUS          18
#define VIRTIO_PCI_ISR             19

struct virtio_pci_cap {
  uint8 cap_vndr;
  uint8 cap_next;
  uint8 cap_len;
  uint8 cfg_type;
  uint8 bar;
  uint8 id;
  uint8 padding[2];
  uint32 offset;
  uint32 length;
};

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
};

static inline volatile uint8 *ecam_fn_base(int bus, int dev, int fn)
{
  uint64 off = ((uint64)bus << 20) | ((uint64)dev << 15) | ((uint64)fn << 12);
  return (volatile uint8 *)(PCI_ECAM_BASE + off);
}

static inline uint8 pci_read8(int bus, int dev, int fn, int off)
{
  return *(volatile uint8 *)(ecam_fn_base(bus, dev, fn) + off);
}
static inline uint16 pci_read16(int bus, int dev, int fn, int off)
{
  return *(volatile uint16 *)(ecam_fn_base(bus, dev, fn) + off);
}
static inline uint32 pci_read32(int bus, int dev, int fn, int off)
{
  return *(volatile uint32 *)(ecam_fn_base(bus, dev, fn) + off);
}
static inline void pci_write16(int bus, int dev, int fn, int off, uint16 v)
{
  *(volatile uint16 *)(ecam_fn_base(bus, dev, fn) + off) = v;
}

static inline uint64 pci_read_bar_addr(int bus, int dev, int fn, int bar_idx)
{
  int off = PCI_BAR0_OFF + bar_idx * 4;
  uint32 lo = pci_read32(bus, dev, fn, off);
  if((lo & 0x1) != 0)
    return (uint64)(lo & ~0x3U); // io bar
  if((lo & 0x6) == 0x4){
    uint32 hi = pci_read32(bus, dev, fn, off + 4);
    return (((uint64)hi << 32) | (lo & ~0xFULL));
  }
  return (uint64)(lo & ~0xFULL);
}

static struct disk {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
  char free[NUM];
  uint16 used_idx;
  struct {
    struct buf *b;
    char status;
  } info[NUM];
  struct virtio_blk_req ops[NUM];
  struct spinlock vdisk_lock;

  int legacy;
  volatile uint8 *legacy_bar;

  volatile struct virtio_pci_common_cfg *common;
  volatile uint8 *notify_base;
  volatile uint8 *isr;
  uint32 notify_off_multiplier;
} disk;

__attribute__((aligned(PGSIZE))) static char legacy_vq[2 * PGSIZE];

static inline uint8 lread8(int off){ return *(volatile uint8 *)(disk.legacy_bar + off); }
static inline uint16 lread16(int off){ return *(volatile uint16 *)(disk.legacy_bar + off); }
static inline uint32 lread32(int off){ return *(volatile uint32 *)(disk.legacy_bar + off); }
static inline void lwrite8(int off, uint8 v){ *(volatile uint8 *)(disk.legacy_bar + off) = v; }
static inline void lwrite16(int off, uint16 v){ *(volatile uint16 *)(disk.legacy_bar + off) = v; }
static inline void lwrite32(int off, uint32 v){ *(volatile uint32 *)(disk.legacy_bar + off) = v; }

static int alloc_desc(void){
  for(int i=0;i<NUM;i++) if(disk.free[i]){ disk.free[i]=0; return i; }
  return -1;
}
static void free_desc(int i){
  if(i>=NUM || disk.free[i]) panic("free_desc");
  disk.desc[i].addr = 0; disk.desc[i].len = 0; disk.desc[i].flags = 0; disk.desc[i].next = 0;
  disk.free[i] = 1;
}
static void free_chain(int i){
  while(1){ int f=disk.desc[i].flags,n=disk.desc[i].next; free_desc(i); if(f & VRING_DESC_F_NEXT) i=n; else break; }
}
static int alloc3_desc(int *idx){
  for(int i=0;i<3;i++){ idx[i]=alloc_desc(); if(idx[i]<0){ for(int j=0;j<i;j++) free_desc(idx[j]); return -1; } }
  return 0;
}

static int virtio_pci_find_blk(int *out_bus, int *out_dev, int *out_fn)
{
  for(int dev=0; dev<32; dev++){
    int fnmax=1;
    if(pci_read8(0,dev,0,PCI_HEADER_TYPE_OFF) & 0x80) fnmax=8;
    for(int fn=0; fn<fnmax; fn++){
      uint16 ven = pci_read16(0,dev,fn,PCI_VENDOR_ID_OFF);
      if(ven == 0xffff) continue;
      uint16 did = pci_read16(0,dev,fn,PCI_DEVICE_ID_OFF);
      uint8 cls = pci_read8(0,dev,fn,PCI_CLASS_OFF);
      uint8 sub = pci_read8(0,dev,fn,PCI_SUBCLASS_OFF);
      if(ven==VIRTIO_PCI_VENDOR_ID && cls==0x01 && sub==0x00 &&
         (did==VIRTIO_PCI_DEVICE_BLK || did==VIRTIO_PCI_DEVICE_BLK_MOD || (did>=0x1000 && did<=0x107f))){
        *out_bus=0; *out_dev=dev; *out_fn=fn; return 0;
      }
    }
  }
  return -1;
}

static void virtio_init_modern(int bus, int dev, int fn)
{
  int got_common=0, got_notify=0, got_isr=0;
  uint8 cap = pci_read8(bus, dev, fn, PCI_CAP_PTR_OFF) & ~0x3;
  while(cap >= 0x40){
    uint8 cap_id = pci_read8(bus, dev, fn, cap+0);
    uint8 cap_next = pci_read8(bus, dev, fn, cap+1) & ~0x3;
    if(cap_id == PCI_CAP_ID_VENDOR){
      struct virtio_pci_cap c;
      c.cfg_type = pci_read8(bus, dev, fn, cap+3);
      c.bar = pci_read8(bus, dev, fn, cap+4);
      c.offset = pci_read32(bus, dev, fn, cap+8);
      uint64 bar = pci_read_bar_addr(bus, dev, fn, c.bar);
      volatile uint8 *base = (volatile uint8 *)((bar & ~0x3ULL) | DMWIN1_MASK);
      if(c.cfg_type == VIRTIO_PCI_CAP_COMMON_CFG){ disk.common = (volatile struct virtio_pci_common_cfg *)(base + c.offset); got_common=1; }
      else if(c.cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG){ disk.notify_base = base + c.offset; disk.notify_off_multiplier = pci_read32(bus,dev,fn,cap+16); got_notify=1; }
      else if(c.cfg_type == VIRTIO_PCI_CAP_ISR_CFG){ disk.isr = base + c.offset; got_isr=1; }
    }
    if(cap_next == cap) break;
    cap = cap_next;
  }
  if(!got_common || !got_notify || !got_isr){
    printf("virtio caps: common=%d notify=%d isr=%d cap_ptr=%d\n", got_common, got_notify, got_isr, pci_read8(bus,dev,fn,PCI_CAP_PTR_OFF));
    panic("virtio-pci caps incomplete");
  }

  disk.common->device_status = 0;
  uint8 status = VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER;
  disk.common->device_status = status;

  disk.common->device_feature_select = 0;
  uint32 f = disk.common->device_feature;
  f &= ~(1u<<VIRTIO_BLK_F_RO);
  f &= ~(1u<<VIRTIO_BLK_F_SCSI);
  f &= ~(1u<<VIRTIO_BLK_F_CONFIG_WCE);
  f &= ~(1u<<VIRTIO_BLK_F_MQ);
  f &= ~(1u<<VIRTIO_F_ANY_LAYOUT);
  f &= ~(1u<<VIRTIO_RING_F_EVENT_IDX);
  f &= ~(1u<<VIRTIO_RING_F_INDIRECT_DESC);
  disk.common->driver_feature_select = 0;
  disk.common->driver_feature = f;

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  disk.common->device_status = status;
  if((disk.common->device_status & VIRTIO_CONFIG_S_FEATURES_OK) == 0)
    panic("virtio FEATURES_OK unset");

  disk.desc = kalloc();
  disk.avail = kalloc();
  disk.used = kalloc();
  if(!disk.desc || !disk.avail || !disk.used) panic("virtio kalloc");
  memset(disk.desc,0,PGSIZE); memset(disk.avail,0,PGSIZE); memset(disk.used,0,PGSIZE);

  disk.common->queue_select = 0;
  if(disk.common->queue_size < NUM) panic("virtio queue too short");
  disk.common->queue_size = NUM;
  disk.common->queue_desc = KV2PA(disk.desc);
  disk.common->queue_driver = KV2PA(disk.avail);
  disk.common->queue_device = KV2PA(disk.used);
  disk.common->queue_enable = 1;

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  disk.common->device_status = status;
}

static void virtio_init_legacy(int bus, int dev, int fn)
{
  uint64 bar = pci_read_bar_addr(bus, dev, fn, 0);
  if(bar == 0) panic("virtio legacy no bar0");
  disk.legacy = 1;
  disk.legacy_bar = (volatile uint8 *)((bar & ~0x3ULL) | DMWIN1_MASK);

  lwrite8(VIRTIO_PCI_STATUS, 0);
  lwrite8(VIRTIO_PCI_STATUS, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

  uint32 f = lread32(VIRTIO_PCI_HOST_FEATURES);
  f &= ~(1u<<VIRTIO_BLK_F_RO);
  f &= ~(1u<<VIRTIO_BLK_F_SCSI);
  f &= ~(1u<<VIRTIO_BLK_F_CONFIG_WCE);
  f &= ~(1u<<VIRTIO_BLK_F_MQ);
  f &= ~(1u<<VIRTIO_F_ANY_LAYOUT);
  f &= ~(1u<<VIRTIO_RING_F_EVENT_IDX);
  f &= ~(1u<<VIRTIO_RING_F_INDIRECT_DESC);
  lwrite32(VIRTIO_PCI_GUEST_FEATURES, f);

  memset(legacy_vq, 0, sizeof(legacy_vq));
  disk.desc = (struct virtq_desc *)legacy_vq;
  disk.avail = (struct virtq_avail *)(legacy_vq + NUM * sizeof(struct virtq_desc));
  disk.used = (struct virtq_used *)(legacy_vq + PGSIZE);

  lwrite16(VIRTIO_PCI_QUEUE_SEL, 0);
  if(lread16(VIRTIO_PCI_QUEUE_NUM) < NUM) panic("virtio legacy queue short");
  lwrite32(VIRTIO_PCI_QUEUE_PFN, (uint32)(KV2PA(legacy_vq) >> PGSHIFT));

  uint8 status = lread8(VIRTIO_PCI_STATUS);
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  lwrite8(VIRTIO_PCI_STATUS, status);

  printf("virtio: using legacy pci fallback\n");
}

void virtio_disk_init(void)
{
  initlock(&disk.vdisk_lock, "virtio_disk");

  int bus, dev, fn;
  if(virtio_pci_find_blk(&bus, &dev, &fn) < 0) panic("virtio-pci blk not found");

  uint16 did = pci_read16(bus, dev, fn, PCI_DEVICE_ID_OFF);
  uint8 cls = pci_read8(bus, dev, fn, PCI_CLASS_OFF);
  uint8 sub = pci_read8(bus, dev, fn, PCI_SUBCLASS_OFF);
  printf("virtio-pci blk @ %d:%d.%d did=%x class=%x/%x\n", bus, dev, fn, did, cls, sub);

  uint16 cmd = pci_read16(bus, dev, fn, PCI_COMMAND_OFF);
  cmd |= (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_IO);
  pci_write16(bus, dev, fn, PCI_COMMAND_OFF, cmd);

  for(int i=0;i<NUM;i++) disk.free[i]=1;

  // did=0x1001 is transitional/legacy virtio-blk on many QEMU versions.
  // Prefer legacy path to avoid mis-detecting modern caps and queue layout.
  if(did == VIRTIO_PCI_DEVICE_BLK){
    virtio_init_legacy(bus, dev, fn);
    return;
  }

  uint16 pstatus = pci_read16(bus, dev, fn, PCI_STATUS_OFF);
  if((pstatus & (1 << 4)) != 0){
    uint8 cap_ptr = pci_read8(bus, dev, fn, PCI_CAP_PTR_OFF);
    if(cap_ptr)
      virtio_init_modern(bus, dev, fn);
    else
      panic("virtio-pci no cap ptr");
  } else {
    panic("virtio-pci no cap-list");
  }
}

static void virtio_disk_complete_used(void)
{
  while(disk.used_idx != disk.used->idx){
    int id = disk.used->ring[disk.used_idx % NUM].id;
    if(disk.info[id].status != 0) panic("virtio_disk status");
    struct buf *b = disk.info[id].b;
    b->disk = 0;
    free_chain(id);
    disk.used_idx += 1;
  }
}

void virtio_disk_rw(struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512);
  int idx[3];
  acquire(&disk.vdisk_lock);
  while(alloc3_desc(idx) != 0) ;

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];
  buf0->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = KV2PA(buf0);
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = KV2PA(b->data);
  disk.desc[idx[1]].len = BSIZE;
  disk.desc[idx[1]].flags = (write ? 0 : VRING_DESC_F_WRITE) | VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff;
  disk.desc[idx[2]].addr = KV2PA(&disk.info[idx[0]].status);
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[2]].next = 0;

  b->disk = 1;
  disk.info[idx[0]].b = b;

  disk.avail->ring[disk.avail->idx % NUM] = idx[0];
  __sync_synchronize();
  disk.avail->idx += 1;
  __sync_synchronize();

  if(disk.legacy){
    lwrite16(VIRTIO_PCI_QUEUE_NOTIFY, 0);
  } else {
    uint16 notify_off = disk.common->queue_notify_off;
    volatile uint16 *notify = (volatile uint16 *)(disk.notify_base +
      ((uint32)notify_off) * disk.notify_off_multiplier);
    *notify = 0;
  }

  while(b->disk == 1)
    virtio_disk_complete_used();

  disk.info[idx[0]].b = 0;
  release(&disk.vdisk_lock);
}

void virtio_disk_intr(void)
{
  acquire(&disk.vdisk_lock);
  if(disk.legacy)
    (void)lread8(VIRTIO_PCI_ISR);
  else
    (void)(*disk.isr);
  virtio_disk_complete_used();
  release(&disk.vdisk_lock);
}
