"""Regenerate the tools/golden ISA sweep fixtures from tools/sweeps sources.

Regenerate:  python3 tools/rv32sweep.py
Check only:  python3 tools/rv32sweep.py --check   (future CI hook)

Each sweep runs to ebreak on a single-hart ISS and must write the PASS
marker 0x50415353 at D-RAM 0x6000. If the ISS state shows the FAIL marker
0x4641494c, the first slot whose result (0x6020+4k) disagrees with the
expected table (.word block at 0x1000) is printed and the exit code is the
slot index plus one.

Outputs per hart in tools/golden:
  <hart>.hex          flat $readmemh image, one lowercase word per line
  <hart>.trace.csv    ISS golden trace, frozen nine-field format, no header
  <hart>.state.txt    final x0-x31, RAM sha256, final pc, steps, halt

Assumption the fixtures encode, to be confirmed at RTL M1: the load port
reaches the I-RAM array (expected table at 0x1000 is read with lw). D-RAM
itself is zero at configuration (CV-5V2), so only this image and the
program's own stores ever define it. Trap-ending behaviour (misaligned
fetch, illegal encodings) is NOT exercised here: a halt ends the program
and no golden continuation exists for it; those semantics are pinned by
the unit vectors in tools/tests/test_rv32iss.py instead.
"""

import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.rv32asm import assemble
from tools.rv32iss import Machine

SWEEPS = ('hart0', 'hart1')
GOLDEN = ROOT / 'tools' / 'golden'
MARKER = 0x50415353
FAIL_MARKER = 0x4641494C
MARKER_ADDR = 0x6000
RESULTS_ADDR = 0x6020
EXPECTED_ADDR = 0x1000
SLOT_COUNT = 26


def build(name):
    source = ROOT / 'tools' / 'sweeps' / f'{name}.s'
    hexfile = GOLDEN / f'{name}.hex'
    assemble(source, hexfile)
    words = [int(line, 16) for line in
             hexfile.read_text().split() if line.strip()]
    image = b''.join(word.to_bytes(4, 'little') for word in words)

    machine = Machine(hart_count=2)
    index = int(name[-1])
    hart = machine.harts[index]
    hart.mem[:len(image)] = image
    while hart.halted is None and machine.steps < machine.max_steps:
        machine.step(index)

    if hart.halted != 'ebreak':
        raise SystemExit(f'{name}: halt reason is {hart.halted}, expected ebreak')
    marker = int.from_bytes(hart.mem[MARKER_ADDR:MARKER_ADDR + 4], 'little')
    if marker != MARKER:
        for index in range(SLOT_COUNT):
            got = int.from_bytes(
                hart.mem[RESULTS_ADDR + 4 * index:RESULTS_ADDR + 4 * index + 4],
                'little')
            exp = words[EXPECTED_ADDR // 4 + index]
            if got != exp:
                print(f'{name}: FAIL marker, first mismatch slot {index}: '
                      f'got {got:08x}, expected {exp:08x}', file=sys.stderr)
                raise SystemExit(index + 1)
        raise SystemExit(f'{name}: FAIL marker but every slot matches')

    trace = ''.join(f'{row}\n' for row in machine.trace)
    state = ''.join(f'x{i}={reg:08x}\n' for i, reg in enumerate(hart.x))
    state += f'pc={hart.pc:08x}\n'
    state += f'steps={machine.steps}\n'
    state += f'halt=ebreak\n'
    state += f'ram_sha256={hashlib.sha256(bytes(hart.mem)).hexdigest()}\n'
    return {'hex': hexfile.read_text(), 'trace.csv': trace,
            'state.txt': state}


def main():
    check = '--check' in sys.argv[1:]
    failures = []
    for name in SWEEPS:
        artifacts = build(name)
        for suffix, text in artifacts.items():
            path = GOLDEN / f'{name}.{suffix}'
            if check:
                if path.read_text() != text:
                    failures.append(str(path))
            else:
                path.write_text(text)
        if not check:
            print(f'{name}: PASS marker, {Machine.__name__} state captured')
    if check and failures:
        print('goldens drifted: ' + ', '.join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
