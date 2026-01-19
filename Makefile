K=kernel
U=user

# 核心目标文件列表
# 之后再手动添加新的 .o 文件
# kernel/Makefile

OBJS = \
  $K/entry.o \
  $K/start.o \
  $K/main.o \
  $K/printf.o \
  $K/console.o \
  $K/uart.o \
  $K/spinlock.o \
  $K/string.o



UNAME_M=$(shell uname -m)
ifeq ($(findstring loongarch64,$(UNAME_M)),loongarch64)
    TOOLPREFIX ?= 
else
    TOOLPREFIX = loongarch64-unknown-linux-gnu-
endif


CC = $(TOOLPREFIX)gcc -march=loongarch64 -mabi=lp64s
AS = $(TOOLPREFIX)gcc -march=loongarch64 -mabi=lp64s
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# 使用 lp64s
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -march=loongarch64 -mabi=lp64s
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-strict-align
CFLAGS += -I.
CFLAGS += -fno-stack-protector
CFLAGS += -fno-pie -no-pie

# 强制 4096 字节对齐，这是 LoongArch 内存管理的基础
LDFLAGS = -z max-page-size=4096

# 使用 $^ 自动化变量，它会自动把所有依赖（即 OBJS）按顺序放进去，避免手动解析变量出错
$K/kernel: $(OBJS) $K/kernel.ld
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

# [cite_start]用户态初始代码编译 
$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

# QEMU 启动配置
QEMU = qemu-system-loongarch64
CPUS := 3

# LoongArch QEMU 必须指定适配的机器类型和驱动总线
QEMUOPTS = -machine virt -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
# 由于目前没有文件系统，先把相关内容注释掉
# QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
# QEMUOPTS += -device virtio-blk-pci,drive=x0

qemu: $K/kernel
	$(QEMU) $(QEMUOPTS)

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$U/initcode $U/initcode.out $K/kernel fs.img \
	mkfs/mkfs .gdbinit $U/usys.S

# 引用其它用户态规则 
ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm

$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

mkfs/mkfs: mkfs/mkfs.c $K/fs.h $K/param.h
	gcc -Werror -Wall -I. -o mkfs/mkfs mkfs/mkfs.c

# 默认包含依赖文件
-include kernel/*.d user/*.d