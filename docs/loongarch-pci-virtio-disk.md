# 在 xv6-loongarch 上把 ramdisk 迁移到 PCI VirtIO Block 的实施指南

你当前项目使用 `kernel/ramdisk.c + kernel/ramdisk.h`（`fs.img` 被编译进内核）作为块设备后端。
在 LoongArch `virt` 机器上，QEMU 的块设备通常应通过 **PCI virtio-blk-pci** 暴露，而不是 `virtio-mmio`。

本文给出一条可落地路径：先验证 QEMU 侧 PCI 设备可见，再在内核实现最小 PCI 枚举和 VirtIO PCI（modern）初始化，最后把 `bio.c` 的后端切换到 `virtio_disk_rw`。

---

## 1. 先确认当前代码的现状

- 当前块设备路径：`bio.c -> ramdiskrw()`。
- 当前初始化：`main.c` 调用 `ramdiskinit()`。
- 项目中虽然有 `virtio_disk.c`，但这是 **MMIO 版本**（依赖 `VIRTIO_MMIO_*` 寄存器），LoongArch QEMU `virt` 不适用。

这意味着你不能直接复用 `virtio_disk.c`，必须引入 PCI 传输层。

---

## 2. QEMU 启动参数（PCI 磁盘）

建议先用如下思路启动（示例，按你的 Makefile 改）：

```bash
qemu-system-loongarch64 \
  -machine virt \
  -kernel kernel/kernel \
  -m 256M -smp 1 -nographic \
  -drive file=fs.img,if=none,format=raw,id=vd0 \
  -device virtio-blk-pci,drive=vd0
```

关键点：
- `-drive ...,id=vd0` 定义后端镜像。
- `-device virtio-blk-pci,drive=vd0` 把它作为 PCI virtio 块设备挂到总线。

---

## 3. 内核端最小实现拆分（推荐顺序）

### Step A：PCI 枚举（只做探测）

先新增一个最小 `pci.c`：
- 扫描 bus/device/function。
- 读取 vendor/device ID。
- 找到 virtio 设备（vendor `0x1af4`，modern blk 常见 device `0x1042`）。
- 启动时把探测结果打印到串口。

这样你可以先证明“客体确实看到了 PCI 磁盘”。

### Step B：解析 VirtIO PCI capability（modern）

virtio-pci modern 不再用 legacy I/O 端口，而是通过 PCI capability 链路暴露：
- `VIRTIO_PCI_CAP_COMMON_CFG`
- `VIRTIO_PCI_CAP_NOTIFY_CFG`
- `VIRTIO_PCI_CAP_ISR_CFG`
- `VIRTIO_PCI_CAP_DEVICE_CFG`

你需要：
1. 从 config space 的 capability list 遍历到 `vendor-specific cap`。
2. 根据 `cfg_type` 记录其 BAR + offset + length。
3. 映射到可访问 VA（LoongArch 里通常通过 DMW1 访问 MMIO）。

### Step C：把现有 virtqueue 逻辑复用

好消息是：你 `virtio_disk.c` 里绝大部分“描述符分配 + 提交 + 中断回收”逻辑可以复用。
主要替换的是“寄存器访问层”：

- 旧：`#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))`（MMIO 固定地址）
- 新：基于 `common_cfg` / `notify` / `isr` 的指针访问。

可抽象为：
- `transport_get_status()/set_status()`
- `transport_set_queue(idx)`
- `transport_set_desc/avail/used(pa)`
- `transport_kick(queue)`

上层读写请求代码保持不变。

### Step D：中断路线

你当前中断控制器是 LS7A/EXTIOI 路径。PCI virtio 中断可以先做简化：
- 先尝试 **轮询 used->idx**（无中断也能工作，性能差但易 bring-up）。
- 功能跑通后再接 MSI/MSI-X 或 INTx。

对于 xv6 这种教学系统，先轮询能显著降低复杂度。

### Step E：切换块设备后端

在功能确认后再替换：
- `main.c`：`ramdiskinit()` -> `virtio_disk_init()`
- `bio.c`：`ramdiskrw()` -> `virtio_disk_rw()`
- Makefile 去掉 `ramdisk.h` 生成依赖（或者保留作为 fallback）。

建议保留 `#ifdef USE_RAMDISK` 开关，便于回退调试。

---

## 4. 最小可行验证（MVP）

先不追求完整中断，只验证“块读”路径：

1. 内核打印：发现 virtio-blk-pci。
2. `virtio_disk_init()` 成功完成 feature negotiation + queue 建立。
3. 在 `fsinit(ROOTDEV)` 前后加日志，确认 `bread(1,1)` 能返回合法 superblock。
4. 能进 shell，`ls` 可列目录。

只要这 4 步通过，说明“磁盘挂载并读取”已经成立。

---

## 5. 你现在就可以做的调试命令

在宿主机（你当前环境）先做静态检查：

```bash
# 看现有调用链是否仍然绑在 ramdisk
rg -n "ramdiskinit|ramdiskrw|virtio_disk_init|virtio_disk_rw" kernel

# 看是否已有任何 PCI 基础设施
rg -n "pci|ecam|msi|msix|capability" kernel
```

在 QEMU monitor（若启用 monitor）可检查 PCI 设备是否出现：

```text
info pci
```

若 `info pci` 能看到 virtio block 设备，而客体内核看不到，问题就集中在“PCI config space 访问实现”。

---

## 6. 常见坑位（LoongArch 特有）

1. **把 MMIO virtio 驱动直接搬过来**：通常会失败，因为寄存器模型已变。
2. **BAR 地址未做 LoongArch DMW 映射**：会访问异常。
3. **先做中断导致复杂度爆炸**：建议先轮询 bring-up。
4. **fs.img 仍内嵌成 ramdisk**：会掩盖真实块设备路径，建议加编译开关避免误判。

---

## 7. 推荐迁移策略（低风险）

- 阶段 1：保留 ramdisk 默认，新增 `USE_VIRTIO_PCI_DISK`（实验开关）。
- 阶段 2：打开开关后仅支持轮询 I/O。
- 阶段 3：补中断与错误处理。
- 阶段 4：删除 ramdisk 依赖。

这样你每一步都能启动内核，不会“一次性改崩”。

---

如果你愿意，我下一步可以直接给你一版“最小 PCI 探测 + virtio-blk-pci 初始化骨架”的补丁清单（`pci.c/pci.h`、`virtio_pci.c`、`bio.c/main.c/Makefile` 具体改哪些行），你可以直接贴进仓库编译验证。
