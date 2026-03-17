# Initial process that execs /init.
# This code runs in user space and is copied into the first user page.

#include "kernel/syscall.h"

.globl start
start:
	pcaddu12i $a0, 0
	ld.d      $a0, $a0, 64
	pcaddu12i $a1, 0
	ld.d      $a1, $a1, 64
	li.w      $a7, SYS_exec
	syscall   0

exit:
	li.w      $a7, SYS_exit
	syscall   0
	b         exit

init:
	.string   "/init"
	.byte     0, 0

argv:
	.dword    init
	.dword    0
	.word     0

initptr:
	.dword    init
argvptr:
	.dword    argv
