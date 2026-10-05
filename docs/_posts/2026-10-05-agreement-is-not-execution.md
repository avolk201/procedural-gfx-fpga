---
layout: default
title: "Agreement Is Not Execution: Vector Review for an RV32I Golden Model"
date: 2026-10-05
description: "How the ISS step 3 and step 4 vector tables were reviewed, what
two independent encoders missed, and what only running could catch."
---

The repository's RV32 work follows a fixed ladder: contract, golden model,
self-checking testbench seen to fail, then RTL. No RV32 RTL exists yet, by
design. This post covers the golden model half of that: steps 3 and 4 of the
ISS, branches, jumps, U-type, fence, and CSR, which took the test suite from
56 to 116 checks. Every number here is reproducible with
`python3 tools/tests/test_rv32iss.py`.

## Two bookkeepers, and what agreement caught

Anything numeric in this project is derived twice from the same printed spec:
once by hand, once by a second reviewer, and only agreement counts. For the
jump vector table, the first full agreement pass caught the encoding layer.
Of 22 committed rows, 7 were right as written. The failures fell into two
hand patterns:

- A JAL at pc 0x1000 with offset -4 was committed as 0xffdfffef. The
  sign-extension nibble had bled into bits 11:8, which belong to rd[4:3], so
  the word decodes as `jal x31, -4`. Correct: 0xffdff0ef. Three rows.
- Every `jalr` with rs1=x2 was committed with the register field at bit 16
  instead of bit 15: 0x000200e7 decodes as rs1=x4. Correct: 0x000100e7.
  Eleven rows, all one slip.

Two more rows were not slips but semantics: a word-aligning "clear" on a JALR
target (the spec clears bit 0 only; 0x2002 stays 0x2002), and a JALR expected
to be pc-relative (it is base-relative; `jalr x1, x0, 8` lands at 8, not at
pc+8). The second one is the same error family as the 2026-09-27 assembler
bug: the base term picking up the wrong origin.

## What agreement missed

Two defect classes survived full word-level agreement and only died on
execution.

First, the committed table placed two instructions at source pcs outside the
hart's 32 KiB RAM window (pc 0x100000 and pc 0xfffffffc). The encodings are
legal, the arithmetic was right, the rows can never retire: the fetch check
halts bus first. Both rows had been agreed twice. Fixed by re-anchoring
(min_m at pc 0x1000 with target 0xFFF01000; wrap_fwd at the last fetchable
word, pc 0x7FFC). The full 2^32 link wrap is now known to be unreachable in
this model, which is a fact about the contract's RAM map, not about RISC-V.

Second, two branch vectors used offset +4 in the not-taken case. A sign
mutation made `blt` compare unsigned and reddened 1 check where the written
prediction said 2: a wrongly taken +4 branch lands exactly on pc+4, the same
observable as not-taken. The vectors moved to +8 (0x0020c463, 0x00109463) and
the design rule is now stated in the ledger: every branch vector's offset
differs from both 0 and +4, so both outcomes of the predicate are
distinguishable in both directions.

## Citations are per revision

Two claims the working notes carried failed re-extraction against the
ratified 20250508 unprivileged volume:

- "JALR uses the original rs1 when rd equals rs1, printed p. 30." No such
  sentence exists in that volume, nor in the 2026-09-24 intermediate
  snapshot. The rule survives on what is printed: 2.5.1 computes the target
  from rs1 first, then writes pc+4 to rd, and Table 3 on p. 32 defines the
  rd=rs1 case.
- The fence mask bit order quoted from memory. The bare fence word for rw,rw
  is 0x0330000f, proven without any bit-order sentence at all: the chapter
  35 listing on p. 610 shows FENCE.TSO with pred=succ=0011 (2.7 says TSO is
  RW,RW) and PAUSE with pred=0001 (2.9 says PAUSE is W). Table rows are
  harder to misremember than prose.

## Convention before it is load-bearing

The spec says a jump to a misaligned target raises
instruction-address-misaligned, reported on the jump itself (2.2, p. 25).
This ISS retires the jump and halts at the next fetch, because this machine
has no trap delivery: halt is terminal, so the reporting location only fixes
the shape of a trace row. That is a deviation, so it was written down as one
(decision D20) while it costs one paragraph instead of a co-simulation
mismatch: the trace format is frozen in the contract, RTL M1 must match the
ISS, and the unaligned vector documents both rows of the convention.

## The apparatus

Each rule above is backed by mutations run on isolated copies, with the
prediction written first: eight step 3 mutations reddening 8, 8, 2, 2, 13,
1, 1, 2 checks, and seven step 4 mutations reddening 0, 3, 6, 1, 1, 8, 3.
The 0 is honest: with one read-only CSR and no read side effects, the rd=x0
"shall not read" rule cannot be observed; it is recorded as untested, not as
coverage. The 3 for N2 replaced a prediction of 2 that got the initial
register contents wrong; the correction is in the ledger next to the result.
The same words now pass through three places: the assembler emits them, the
encoder round-trips them, the ISS executes them.

## Status

The golden model covers base RV32I plus Zicsr, 116 checks in CI. The per-hart
RAM budget closed at 24 KiB I / 8 KiB D, 64 of 553 M10K blocks, with the
stack at the top of D-RAM and overflow falling into the bus hole, which the
model already halts on. Next rung: the self-checking testbench, seen to fail
before any RTL exists. See commits d126bf2 through 8029ede and
docs/devlog.md B21.
