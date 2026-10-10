# hart0 ISA sweep. Contract 4b: self-checking, final state is the score,
# program ends in ebreak. Expected table at 0x1000 (I-RAM via the load
# port), results at D 0x6020+4k, staging 0x60A0.., marker at 0x6000.
# Fail paths write 0x4641494c at the marker slot instead of 0x50415353.

  li x27, 0x6000
  li x2, 0x8000

  li x5, 0xFFFFFFFF
  li x6, 1
  li x7, 0x80000000
  li x8, 0x7FFFFFFF
  li x9, 0
  li x10, 0x12345678
  li x11, 0x00FF00FF
  li x12, 0xFF00FF00
  li x13, 5

  sw x10, 0xA0(x27)
  li x3, 0xDEADBEEF
  sw x3, 0xA4(x27)
  li x3, 0xAAAAAAAA
  sw x3, 0xA8(x27)
  sw x3, 0xAC(x27)

  add x3, x5, x6
  sw x3, 0x20(x27)
  sub x3, x6, x5
  sw x3, 0x24(x27)
  sll x3, x5, x13
  sw x3, 0x28(x27)
  slt x3, x7, x6
  sw x3, 0x2C(x27)
  sltu x3, x5, x6
  sw x3, 0x30(x27)
  xor x3, x11, x12
  sw x3, 0x34(x27)
  li x14, 4
  srl x3, x7, x14
  sw x3, 0x38(x27)
  li x14, 1
  sra x3, x7, x14
  sw x3, 0x3C(x27)
  and x3, x11, x12
  sw x3, 0x40(x27)
  or x3, x11, x12
  sw x3, 0x44(x27)
  addi x3, x6, -1
  sw x3, 0x48(x27)
  slli x3, x11, 16
  sw x3, 0x4C(x27)
  slti x3, x7, -1
  sw x3, 0x50(x27)
  sltiu x3, x6, -1
  sw x3, 0x54(x27)
  srli x3, x7, 31
  sw x3, 0x58(x27)
  srai x3, x7, 31
  sw x3, 0x5C(x27)

  csrrs x1, mhartid, x0
  sw x1, 0x60(x27)

  la x21, afterjal
  jal x22, farlabel
afterjal:
  j endchk
farlabel:
  bne x22, x21, fail
endchk:

  la x23, jtarget
  addi x23, x23, -1
  auipc x25, 0
  jalr x24, x23, 1
jtarget:
  addi x25, x25, 8
  bne x24, x25, fail
  li x4, 1
  sw x4, 0x64(x27)

  la x21, backjal
  addi x21, x21, 4
backjal:
  jal x22, fwdland
  j endchk3
fwdland:
  bne x22, x21, fail
endchk3:

  lui x3, 0x12345
  sw x3, 0x68(x27)
  lw x3, 0xA0(x27)
  sw x3, 0x6C(x27)
  lb x3, 0xA4(x27)
  sw x3, 0x70(x27)
  lbu x3, 0xA4(x27)
  sw x3, 0x74(x27)
  lh x3, 0xA6(x27)
  sw x3, 0x78(x27)
  lhu x3, 0xA6(x27)
  sw x3, 0x7C(x27)
  li x4, 0xBB
  sh x4, 0xAA(x27)
  lw x3, 0xA8(x27)
  sw x3, 0x80(x27)
  li x4, 0x11
  sb x4, 0xAC(x27)
  lw x3, 0xAC(x27)
  sw x3, 0x84(x27)

  bge x27, x7, bs1
  j fail
bs1:
  blt x7, x5, bs2
  j fail
bs2:
  bgeu x5, x6, bs3
  j fail
bs3:
  bltu x6, x5, bs4
  j fail
bs4:
  beq x9, x0, bs5
  j fail
bs5:
  bne x10, x0, bs6
  j fail
bs6:

  addi x15, x27, 0x20
  la x16, exp
  li x19, 26
  li x28, 0
cmp:
  lw x17, 0(x15)
  lw x18, 0(x16)
  bne x17, x18, fail
  addi x15, x15, 4
  addi x16, x16, 4
  addi x28, x28, 1
  bltu x28, x19, cmp

  li x30, 0x50415353
  sw x30, 0(x27)
  fence
  ebreak
fail:
  li x30, 0x4641494C
  sw x30, 0(x27)
  ebreak

  .org 0x1000
exp:
  .word 0x00000000
  .word 0x00000002
  .word 0xFFFFFFE0
  .word 0x00000001
  .word 0x00000000
  .word 0xFFFFFFFF
  .word 0x08000000
  .word 0xC0000000
  .word 0x00000000
  .word 0xFFFFFFFF
  .word 0x00000000
  .word 0x00FF0000
  .word 0x00000001
  .word 0x00000001
  .word 0x00000001
  .word 0xFFFFFFFF
  .word 0x00000000
  .word 0x00000001
  .word 0x12345000
  .word 0x12345678
  .word 0xFFFFFFEF
  .word 0x000000EF
  .word 0xFFFFDEAD
  .word 0x0000DEAD
  .word 0x00BBAAAA
  .word 0xAAAAAA11
