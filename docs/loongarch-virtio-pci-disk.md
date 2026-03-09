# LoongArch (QEMU) 上将 xv6 从 ramdisk 切换到 PCI 磁盘（virtio-blk-pci）

当前仓库的文件系统 I/O 走的是 `ramdisk`：
- `main()` 里初始化的是 `ramdiskinit()`。
- `bread()/bwrite()` 调用的是 `ramdiskrw()`。
- `Makefile` 会把 `fs.img` 转成 `kernel/ramdisk.h` 编译进内核。

这条路径适合 bring-up，但不是“真实块设备”。

> 你提到的关键点是对的：`qemu-system-loongarch64` 默认常见用法是 PCI 设备（`virtio-blk-pci`），而不是 RISC-V 那种固定 MMIO virtio 设备。

---

## 目标架构

建议用 **virtio over PCI（modern）**，并在内核中实现：

1. PCI 枚举（读配置空间，找到 class=0x01/subclass=0x00 或 vendor/device=1af4:1042/1041）
2. BAR 映射（common cfg / notify / ISR / device cfg）
3. virtqueue 初始化（desc/avail/used）
4. 中断接入（优先 MSI-X，不行可先 INTx）
5. 在 `bio.c` 把 `ramdiskrw()` 切到 `virtio_disk_rw()`

---

## 分阶段迁移（推荐）

### 第 0 阶段：保留 ramdisk 兜底

先不要删除 `ramdisk.c`，保留一个编译开关：
- `CONFIG_RAMDISK=1` 走老路径
- `CONFIG_VIRTIO_PCI_BLK=1` 走新路径

这样每次改动都可回退。

### 第 1 阶段：QEMU 命令行切换到 PCI 磁盘

把运行参数改成类似：

```bash
qemu-system-loongarch64 \
  -machine virt \
  -kernel kernel/kernel \
  -m 256M -smp 1 -nographic \
  -drive file=fs.img,if=none,format=raw,id=x0 \
  -device virtio-blk-pci,drive=x0
```

说明：
- 先不要 `-initrd fs.img`（那是 ramdisk 方案）。
- 使用 `virtio-blk-pci`，让磁盘通过 PCI 暴露。

### 第 2 阶段：接入 PCI 枚举

新增 `kernel/pci.c`，至少实现：
- 读取配置空间 `vendor/device/class/subclass/BAR/capability list`
- 遍历 bus 0（先简单做单 bus）
- 找到 virtio-blk PCI function 并记录其 BAR 与能力结构

> LoongArch `virt` 机型下的 PCI ECAM/MMCONFIG 物理地址请以 QEMU 文档或设备树为准，再映射到你内核可访问 VA（你项目中 DMW1 已用于设备寄存器访问）。

### 第 3 阶段：virtio-pci 现代接口初始化

virtio modern PCI 关键 capability：
- `VIRTIO_PCI_CAP_COMMON_CFG`
- `VIRTIO_PCI_CAP_NOTIFY_CFG`
- `VIRTIO_PCI_CAP_ISR_CFG`
- `VIRTIO_PCI_CAP_DEVICE_CFG`

流程：
1. reset device status
2. ACKNOWLEDGE + DRIVER
3. 读写 feature bits（屏蔽你不支持的）
4. FEATURES_OK
5. 配置 queue（queue_select/size/desc/avail/used）
6. 使能 queue
7. DRIVER_OK

你仓库已有 `kernel/virtio_disk.c` 的“队列组织 + 请求收发”代码可复用（desc/avail/used 与三段描述符链逻辑）。
差异主要在“寄存器访问层”：
- 现在是 MMIO `R(r)`
- 需要改为 PCI capability 对应结构寄存器读写

### 第 4 阶段：中断路径打通

当前 `trap.c` 只处理 UART / timer。
需要加上块设备中断来源识别并调用：
- `virtio_disk_intr()`

建议先做“轮询 + 定时触发”验证 I/O 正确，再接中断，最后去掉轮询。

### 第 5 阶段：切换文件系统读写入口

将 `bio.c` 中：
- `ramdiskrw(b, 0/1)`

替换为：
- `virtio_disk_rw(b, 0/1)`

并在 `main()` 中改为初始化 `virtio_disk_init()`（或 `pci_virtio_blk_init()`）。

验证顺序：
1. 只读超级块成功
2. 能 `ls`
3. 能创建文件并重启后仍存在（确认非内存盘）

---

## 你仓库里目前“为什么还没走到 virtio”

从代码现状看：
- `Makefile` 的 `OBJS` 包含 `ramdisk.o`，不包含 `virtio_disk.o`。
- `main()` 初始化的是 `ramdiskinit()`。
- `bio.c` 读写路径固定调用 `ramdiskrw()`。
- `virtio_disk.c` 是“MMIO virtio”驱动模型，不是 PCI 版本。

所以你现在“系统能跑但磁盘是内存盘”是完全符合当前实现状态的。

---

## 最小可行改造建议（MVP）

如果你希望最快看到“真正块设备 I/O”，建议按这个最小路径：

1. 保留 `virtio_disk.c` 的 queue/request 逻辑，抽出 `virtio_ops`：
   - `read32/write32`、`queue_set_addr`、`notify`、`ack_irq`
2. 新增 `virtio_pci_transport.c` 提供这些 ops
3. `bio.c/main.c` 切到 `virtio_disk_*`
4. QEMU 改成 `virtio-blk-pci`

这样复用率最高，改动面最小。

---

## 调试建议

- 启动加 `-d guest_errors`
- 在内核打印：
  - 枚举到的 PCI 设备（bus:dev.fn/vendor:device/class）
  - virtio feature 协商结果
  - queue PFN/addr
  - 每次提交请求的 sector 与完成状态
- 出现卡死优先查：
  - 描述符地址是否是设备可 DMA 的物理地址
  - notify offset 计算是否正确
  - ISR ACK 是否正确
  - 中断号是否真的 routed 到 extioi/apic 这条路径

---

## 一句话结论

你的方向应当是：**把“virtio-mmio 磁盘驱动”改为“virtio-pci 磁盘驱动（transport 层替换）”，并把 `main/bio/Makefile/QEMUOPTS` 从 ramdisk 路径切过去**。
