set confirm off
set pagination off
set disassemble-next-line auto

define bcore
  break main
  break usertrap
  break kerneltrap
  break devintr
  break disk_intr
end

document bcore
Set a default breakpoint set for xv6 kernel debugging.
end

define regs
  info registers pc sp ra a0 a1 a2 a3 a4 a5 a6 a7
end

document regs
Show key LoongArch registers.
end
