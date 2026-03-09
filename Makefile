# =========================================================
# Directory definitions
# =========================================================
K = kernel
U = user

# =========================================================
# Kernel object files
# =========================================================
OBJS = \
  $K/entry.o \
  $K/main.o \
  $K/printf.o \
  $K/console.o \
  $K/uart.o \
  $K/spinlock.o \
  $K/sleeplock.o \
  $K/kalloc.o \
  $K/vm.o \
  $K/trap.o \
  $K/kernelvec.o \
  $K/proc.o \
  $K/swtch.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/sysfile.o \
  $K/tlbrefill.o \
  $K/merror.o \
  $K/uservec.o \
  $K/apic.o \
  $K/extioi.o \
  $K/pipe.o \
  $K/fs.o \
  $K/file.o \
  $K/exec.o \
  $K/log.o \
  $K/ramdisk.o \
  $K/bio.o \
  $K/string.o

# =========================================================
# Toolchain detection
# =========================================================
UNAME_M := $(shell uname -m)

ifeq ($(findstring loongarch64,$(UNAME_M)),loongarch64)
TOOLPREFIX :=
else
TOOLPREFIX = loongarch64-unknown-linux-gnu-
endif

CC      = $(TOOLPREFIX)gcc
AS      = $(TOOLPREFIX)gcc
LD      = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# =========================================================
# Compiler flags
# =========================================================
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -march=loongarch64 -mabi=lp64s
CFLAGS += -ffreestanding -fno-common -nostdlib
CFLAGS += -fno-stack-protector
CFLAGS += -fno-pie -no-pie
CFLAGS += -I.

LDFLAGS = -z max-page-size=4096

# =========================================================
# Default target
# =========================================================
all: fs.img $K/kernel

# =========================================================
# Kernel build
# =========================================================
$K/kernel: $(OBJS) $K/kernel.ld
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

# =========================================================
# User library
# =========================================================
ULIB = \
	$U/ulib.o \
	$U/usys.o \
	$U/printf.o \
	$U/umalloc.o

# =========================================================
# Generic user program rule
# =========================================================
_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

# =========================================================
# System call generation
# =========================================================
$U/usys.S: $U/usys.pl
	perl $U/usys.pl > $U/usys.S

$U/usys.o: $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

# =========================================================
# Special rule: forktest
# =========================================================
$U/_forktest: $U/forktest.o
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest \
		$U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm

# =========================================================
# Special rule: shell
# =========================================================
$U/_sh: $U/sh.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_sh $^
	$(OBJDUMP) -S $U/_sh > $U/sh.asm

# =========================================================
# mkfs
# =========================================================
mkfs/mkfs: mkfs/mkfs.c $K/fs.h $K/param.h
	gcc -Wall -Werror -I. -o mkfs/mkfs mkfs/mkfs.c

# =========================================================
# User programs
# =========================================================
UPROGS = \
	$U/_cat \
	$U/_echo \
	$U/_grep \
	$U/_init \
	$U/_kill \
	$U/_ln \
	$U/_ls \
	$U/_mkdir \
	$U/_rm \
	$U/_sh \
	$U/_wc \
	$U/_zombie \
	$U/_forktest \
	$U/_stressfs \
	$U/_dorphan \
	$U/_forphan \
	$U/_logstress \
	$U/_grind\
   #$U/_usertests \

# =========================================================
# File system image
# =========================================================
fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)
	xxd -i fs.img > kernel/ramdisk.h

# =========================================================
# QEMU
# =========================================================
QEMU = qemu-system-loongarch64
CPUS = 1

QEMUOPTS = -machine virt
QEMUOPTS += -kernel $K/kernel
QEMUOPTS += -m 256M
QEMUOPTS += -smp $(CPUS)
QEMUOPTS += -nographic

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

qemu: $K/kernel fs.img
	$(QEMU) $(QEMUOPTS)

# =========================================================
# Dependency include
# =========================================================
-include kernel/*.d user/*.d

# =========================================================
# Clean
# =========================================================
clean:
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg
	rm -f */*.o */*.d */*.asm */*.sym
	rm -f $K/kernel fs.img
	rm -f mkfs/mkfs
	rm -f $U/usys.S
	rm -f $(UPROGS)

# Prevent deleting intermediate .o files
.PRECIOUS: %.o