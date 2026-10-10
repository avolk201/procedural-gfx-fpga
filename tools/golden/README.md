# tools/golden: ISS sweep fixtures (contract 4b)

Regenerate: `python3 tools/rv32sweep.py`
Drift check (future CI hook): `python3 tools/rv32sweep.py --check`

Files, per hart (`hart0`, `hart1`):

- `.hex`: flat $readmemh image, one lowercase 32-bit word per line,
  contiguous from address 0. Text under 0x1000; the expected table sits at
  0x1000 in the I-RAM region; D-RAM (0x6000+) is zero in the image and is
  defined only by the program's own stores.
- `.trace.csv`: ISS retirement/halt trace in the frozen nine-field format
  (contract section 9), no header, one hart column value per file (`0` or
  `1`). 298 rows, ending in the `halt:ebreak` row.
- `.state.txt`: final `x0`-`x31`, `pc`, `steps`, `halt`, `ram_sha256` over
  the full 32 KiB private RAM. This is the M1 scoreboard target: RTL must
  reproduce every byte.

Protocol: PASS writes `0x50415353` at `0x6000`, FAIL writes `0x4641494c`
at `0x6000` then ebreaks. Every check failure paths to `fail`, so a broken
check cannot silently pass. `rv32sweep.py` refuses unless both harts show
PASS and reports the first mismatching result slot (index and values) when
they do not; its exit code is that slot index plus one, 0 on success.

The 26 result slots at `0x6020 + 4k`: k0-k15 the register OP/OP-IMM set
(add wrap, sub, sll, slt, sltu, xor, srl, sra, and, or, addi, slli, slti,
sltiu, srli, srai), k16 mhartid (0 on hart0, 1 on hart1), k17 a
jalr-landing flag written at the jump target itself, k18 lui, k19 lw
identity, k20-k23 lane loads from the `0xDEADBEEF` staging word (lb, lbu,
lh, lhu), k24-k25 lane-merge reads after sh/sb into the `0xAAAAAAAA`
staging words. Between arith and the compare loop the program exercises
`la` (AUIPC+ADDI) at real pcs, forward `jal` with link check against a
label, `jalr` with an odd base clearing bit 0 (link compared against
`auipc+8`), a backward `jal` link check, all six branch opcodes in the
taken direction, and `fence` immediately before the final ebreak. The
compare loop itself is the backward-taken `bltu` plus the loop-exit
not-taken `bltu`/`bne`.

Proven to bite, 2026-10-05, isolated /tmp copy: corrupting the sra expected
word (`0xC0000000` to `0x00000000`, the sra-versus-srl failure mode) flipped
the golden marker to FAIL and made the generator report
`first mismatch slot 7: got c0000000, expected 00000000`, exit 8.

Assumptions these fixtures bake in, to be confirmed before M1 trusts them:
the load port reaches the I-RAM array (the compare loop reads its expected
table with `lw` from 0x1000; a strict Harvard core without that port fails
here by design, and the fix is the table's home, not the protocol).
Trap-ending semantics (misaligned fetch, illegal encodings, D20 jump-then-
fetch halt) are NOT in these sweeps: a halt ends the program and no golden
continuation exists; those stay pinned by the unit vectors in
tools/tests/test_rv32iss.py.

Status: the expected table and the slot assignments are a 2026-10-05
derivation, cross-checked against the ISS and an independent raw-arithmetic
script (26/26 agree). Per the two-bookkeeper protocol they are not golden
until the page re-derivation pass confirms each word against the printed
sections. The ISS they run on is itself proven by the 116-check unit suite.
