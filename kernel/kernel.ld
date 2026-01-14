OUTPUT_ARCH( "loongarch" )
ENTRY( _entry )

SECTIONS
{
  /*
   * 保留2MB的地址用作其他部分，在此后用作内核内存镜像的虚拟地址
   * where qemu's -kernel jumps.
   */
  . = 0x9000000000200000;

.text : {
  kernel/entry.o(_entry)   /* LoongArch 的入口汇编文件 */
  *(.text .text.*)
  . = ALIGN(0x1000);

  _keentry = .;            /* kernel exception entry */
  *(k_eentry)
  . = ALIGN(0x1000);

  _ueentry = .;            /* user exception entry */
  *(u_eentry)
  . = ALIGN(0x1000);

  _tlbrentry = .;          /* TLB refill entry */
  *(tlbrentry)
  . = ALIGN(0x1000);

  _merrentry = .;          /* machine error entry */
  *(merrentry)
  . = ALIGN(0x1000);

  PROVIDE(etext = .);
}


  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}
