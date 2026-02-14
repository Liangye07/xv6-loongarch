#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "loongarch.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// 文件系统相关的桩函数
void iinit() {}
struct inode* namei(char* path) { return 0; }
struct inode* idup(struct inode* ip) { return 0; }
void iput(struct inode* ip) {}
void begin_op() {}
void end_op() {}

// 文件操作相关的桩函数
struct file* filedup(struct file* f) { return 0; }
void fileclose(struct file* f) {}

// 其他缺失的模块
void plicinit() {}
void plicinithart() {}
int kexec(char* path, char** argv) { return -1; }


// 补充一些可能缺失的磁盘/中断相关函数
void virtio_disk_init(void) {}
void virtio_disk_rw(struct buf *b, int write) {}
void virtio_disk_intr(void) {}


// 2. 修复 initcode 符号缺失
// 链接器在 proc.c 编译生成的代码中寻找这些符号来加载第一个用户进程
// 既然我们现在不跑用户进程，随便给它们定义一个空字节即可
uchar _binary_initcode_start[] = { 0 };
uchar _binary_initcode_size[] = { 0 };

