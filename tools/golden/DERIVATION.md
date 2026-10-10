# Expected-word derivation record for the hart sweeps

Two independent legs, per the two-bookkeeper protocol. Leg A: raw-arithmetic
script written from the spec definitions without importing tools/ (run
2026-10-05, 26/26). Leg B: printed-page re-derivation, 2026-10-10, sentences
extracted from the local PDFs (unprivileged and privileged volumes,
20250508), 26/26. Both legs executed under owner delegation before the
fixtures were called golden; agreement is recorded here.

Staged inputs (from the sweep prologue): x5=FFFFFFFF, x6=1, x7=80000000,
x10=12345678, x11=00FF00FF, x12=FF00FF00, x13=5, x14=4 then 1; memory
staging A0=12345678, A4=DEADBEEF, A8=AAAAAAAA, AC=AAAAAAAA (D offsets off
0x6000).

| k | op | rule (source, printed page) | arithmetic | expected |
|---|----|------------------------------|------------|----------|
| 0 | add x5,x6 | "performs the addition of rs1 and rs2 ... Overflows are ignored and the low XLEN bits ... written to rd" (unpriv 2.4, p. 29) | FFFFFFFF+1 = 1_00000000, low 32 | 00000000 |
| 1 | sub x6,x5 | "SUB performs the subtraction of rs2 from rs1" (p. 29) | 1-FFFFFFFF = 1-(-1) | 00000002 |
| 2 | sll x5,x13 | "shifts ... by the shift amount held in the lower 5 bits of register rs2"; "SLLI is a logical left shift (zeros ... lower bits)" (p. 29) | FFFFFFFF<<5, low 32 | FFFFFFE0 |
| 3 | slt x7,x6 | "SLT and SLTU perform signed and unsigned compares respectively, writing 1 to rd if rs1 < rs2" (p. 29) | -2147483648 < 1 | 00000001 |
| 4 | sltu x5,x6 | same sentence, unsigned | 4294967295 < 1 | 00000000 |
| 5 | xor x11,x12 | "AND, OR, and XOR perform bitwise logical operations" (p. 29) | 00FF00FF ^ FF00FF00 | FFFFFFFF |
| 6 | srl x7,x14(4) | "SRLI is a logical right shift (zeros ... upper bits)" (p. 29) | 80000000>>4 | 08000000 |
| 7 | sra x7,x14(1) | "SRAI is an arithmetic right shift (the original sign bit is copied into the vacated upper bits)" (p. 29) | -2147483648>>1 | C0000000 |
| 8 | and x11,x12 | p. 29 | 00FF00FF & FF00FF00 | 00000000 |
| 9 | or x11,x12 | p. 29 | 00FF00FF \| FF00FF00 | FFFFFFFF |
| 10 | addi x6,-1 | ADDI adds the sign-extended immediate (2.4, p. 28) | 1+(-1) | 00000000 |
| 11 | slli x11,16 | "shift amount ... encoded in the lower 5 bits of the I-immediate field" (p. 28); low 32 bits kept (p. 29) | 00FF00FF<<16 truncated | 00FF0000 |
| 12 | slti x7,-1 | "places the value 1 in rd if rs1 is less than the sign-extended immediate when both are treated as signed" (p. 28) | -2147483648 < -1 | 00000001 |
| 13 | sltiu x6,-1 | "SLTIU ... compares the values as unsigned numbers (i.e., the immediate is first sign-extended to XLEN bits then treated as an unsigned number)" (p. 28) | 1 < FFFFFFFF | 00000001 |
| 14 | srli x7,31 | p. 29 logical | 80000000>>31 | 00000001 |
| 15 | srai x7,31 | p. 29 arithmetic, sign copied | -2147483648>>31 = -1 | FFFFFFFF |
| 16 | csrrs mhartid | "read-only register containing the integer ID of the hardware thread running the code" (priv 3.1.5, p. 28) + "one hart to have a known hart ID of zero" (p. 29) | hart0 id / hart1 id | 00000000 / 00000001 |
| 17 | landing flag | program literal stored at the jalr target word; value is that the path executed | li 1; sw | 00000001 |
| 18 | lui 0x12345 | "places the 32-bit U-immediate value into ... rd, filling in the lowest 12 bits with zeros" (p. 29) | 12345<<12 | 12345000 |
| 19 | lw A0 | "LW loads a 32-bit word" (2.6, p. 34) | identity of staging | 12345678 |
| 20 | lb A4+0 | "LB and LBU are defined analogously" to the LH/LHU sign/zero sentence (p. 34); byte order little-endian, least-significant byte at lowest address (p. 34) | byte0 of DEADBEEF = EF, sign bit set | FFFFFFEF |
| 21 | lbu A4+0 | same, zero-extends (p. 34) | EF | 000000EF |
| 22 | lh A4+2 | "LH loads a 16-bit value ... then sign-extends to 32-bits" (p. 34) | bytes AD,DE little-endian = DEAD, sign set | FFFFDEAD |
| 23 | lhu A4+2 | "LHU ... zero extends" (p. 34) | DEAD | 0000DEAD |
| 24 | sh merge, lw A8 | "SH ... store ... 16-bit ... values from the low bits of register rs2" (p. 34); lane bytes only | AAAAAAAA with bytes[2:3] := BB,00, re-read little-endian | 00BBAAAA |
| 25 | sb merge, lw AC | "SB ... 8-bit" (p. 34) | AAAAAAAA with byte[0] := 11 | AAAAAA11 |

Status: GOLDEN as of 2026-10-10 (both legs agree with the literals in
tools/sweeps/*.s and with the fixtures in tools/golden/). Any future change
to a slot re-runs both legs before the fixtures regenerate.
