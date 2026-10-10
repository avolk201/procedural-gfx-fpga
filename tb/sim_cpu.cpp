// tb/sim_cpu.cpp
// M1 cosimulation harness for rv32_core (docs/cpu-contract.md 4b/4c, D23).
//
// The golden is tools/golden/hart0.*: the frozen nine-field trace, the
// final register file, and the final RAM image, all produced by the ISS
// (tools/rv32sweep.py). This tb compares three things and nothing else:
//   1. the row stream rebuilt from the retire tap, string for string;
//   2. x0-x31 replayed from the retire rows against the golden state;
//   3. RAM replayed by applying store rows to the boot image, byte for
//      byte against the golden ram.bin, first bad address reported.
// No backdoors: nothing inside the core is public, the section 9 format
// is the single source of truth (contract 4c).
//
// Until M1 lands, the DUT is the ports-only stub rtl/cpu/rv32_core.sv and
// this tb MUST fail via the watchdog. That red is the artifact: a tb that
// cannot fail against nothing is nothing. `make sim_cpu` is deliberately
// outside regress and coverage until it goes green.
//
// Watchdog arithmetic (house rule: expected duration is informational,
// the watchdog is a hang detector with margin, D11): golden rows N; a
// two-stage core retires at worst one row per 4 clocks (fetch bubble plus
// redirect), so expected ~= POR_SIM + 4N + slack. The watchdog is 20x
// that because the M1 microarchitecture is not pinned yet.
//
// Exit code is the interface: 0 only if every check passed. Trace echo: -v.
//
// Usage: sim_cpu <firmware.hex> <golden.trace.csv> <golden.state.txt>
//                <golden.ram.bin> [-v]

#include "Vrv32_core.h"
#include "verilated.h"
#include "verilated_cov.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static constexpr uint32_t RAM_BYTES = 0x8000;   // contract section 4
static constexpr int POR_SIM_CYCLES = 8;        // matches -GPOR_CYCLES=8
static constexpr int RESET_HOLD_CYCLES = 4;

static bool g_verbose = false;
static int g_checks_run = 0;
static int g_fail_count = 0;

static void check(bool ok, const char *name) {
    ++g_checks_run;
    if (!ok) ++g_fail_count;
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
}

static std::vector<std::string> read_lines(const char *path) {
    std::vector<std::string> out;
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::exit(2);
    }
    std::string line;
    while (std::getline(in, line))
        if (!line.empty()) out.push_back(line);
    return out;
}

static std::vector<uint8_t> read_bytes(const char *path, size_t expect) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::exit(2);
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    if (buf.size() != expect) {
        std::fprintf(stderr, "%s: got %zu bytes, expected %zu\n", path,
                     buf.size(), expect);
        std::exit(2);
    }
    return buf;
}

// Boot image: one lowercase hex word per line, loaded little-endian from
// address 0, rest of RAM zero (D-RAM is zero at configuration, contract
// section 3).
static std::vector<uint8_t> load_image(const char *path) {
    std::vector<uint8_t> ram(RAM_BYTES, 0);
    size_t addr = 0;
    for (const auto &word_text : read_lines(path)) {
        uint32_t word = static_cast<uint32_t>(std::strtoul(word_text.c_str(),
                                                           nullptr, 16));
        if (addr + 4 > RAM_BYTES) {
            std::fprintf(stderr, "%s: image overruns RAM at %zu\n", path,
                         addr);
            std::exit(2);
        }
        for (int b = 0; b < 4; ++b)
            ram[addr + b] = static_cast<uint8_t>((word >> (8 * b)) & 0xFF);
        addr += 4;
    }
    return ram;
}

static const char *halt_name(uint8_t reason) {
    switch (reason) {
        case 0: return "ebreak";
        case 1: return "ecall";
        case 2: return "illegal";
        case 3: return "bus";
        default: return "???";
    }
}

// Mnemonic rebuild per contract section 9: loads carry sign/size, stores
// carry size only. Any other size is a protocol violation and yields a
// string that cannot match the golden.
static const char *mem_mnemonic(bool store, uint8_t size, bool sign) {
    if (store) {
        if (size == 1) return "sb";
        if (size == 2) return "sh";
        if (size == 4) return "sw";
        return "???";
    }
    if (size == 1) return sign ? "lb" : "lbu";
    if (size == 2) return sign ? "lh" : "lhu";
    if (size == 4) return "lw";
    return "???";
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0) { g_verbose = true; continue; }
        args.emplace_back(argv[i]);
    }
    if (args.size() != 4) {
        std::fprintf(stderr,
                     "usage: sim_cpu <firmware.hex> <golden.trace.csv> "
                     "<golden.state.txt> <golden.ram.bin> [-v]\n");
        return 2;
    }

    const std::vector<std::string> golden_rows = read_lines(args[1].c_str());
    const std::vector<uint8_t> golden_ram = read_bytes(args[3].c_str(),
                                                       RAM_BYTES);
    uint32_t golden_regs[32] = {0};
    for (const auto &line : read_lines(args[2].c_str())) {
        int index = -1;
        unsigned value = 0;
        if (std::sscanf(line.c_str(), "x%d=%x", &index, &value) == 2 &&
            index >= 0 && index < 32)
            golden_regs[index] = value;
    }

    const size_t n_rows = golden_rows.size();
    const long expected_cycles =
        POR_SIM_CYCLES + RESET_HOLD_CYCLES + 4L * static_cast<long>(n_rows)
        + 16;
    const long watchdog_cycles = expected_cycles * 20;

    std::vector<uint8_t> ram = load_image(args[0].c_str());
    uint32_t regs[32] = {0};
    std::vector<std::string> rows;
    rows.reserve(n_rows);

    Vrv32_core dut;
    dut.clk_i = 0;
    dut.rst_n_i = 0;
    dut.eval();

    bool halted = false;
    long cycle = 0;
    char line[256];

    while (cycle < watchdog_cycles && !halted) {
        if (cycle == RESET_HOLD_CYCLES) dut.rst_n_i = 1;
        dut.clk_i = 1;
        dut.eval();
        ++cycle;

        if (dut.retire_valid_o) {
            if (dut.retire_mem_valid_o && dut.retire_mem_store_o) {
                const uint32_t addr = dut.retire_mem_addr_o;
                const uint32_t size = dut.retire_mem_size_o;
                const uint32_t value = dut.retire_mem_value_o;
                std::snprintf(line, sizeof line, "0,%08x,%08x,,,%s,%08x,%u,%08x",
                              dut.retire_pc_o, dut.retire_word_o,
                              mem_mnemonic(true, static_cast<uint8_t>(size),
                                           false),
                              addr, size, value);
                for (uint32_t i = 0; i < size && i < 4; ++i) {
                    const uint32_t off = addr + i;
                    if (off < RAM_BYTES)
                        ram[off] = static_cast<uint8_t>(
                            (value >> (8 * ((addr & 3) + i))) & 0xFF);
                }
            } else if (dut.retire_mem_valid_o) {
                std::snprintf(line, sizeof line,
                              "0,%08x,%08x,%u,%08x,%s,%08x,%u,%08x",
                              dut.retire_pc_o, dut.retire_word_o,
                              dut.retire_rd_o, dut.retire_rd_value_o,
                              mem_mnemonic(false,
                                           static_cast<uint8_t>(
                                               dut.retire_mem_size_o),
                                           dut.retire_mem_sign_o != 0),
                              dut.retire_mem_addr_o, dut.retire_mem_size_o,
                              dut.retire_mem_value_o);
                regs[dut.retire_rd_o & 31] = dut.retire_rd_value_o;
            } else {
                std::snprintf(line, sizeof line, "0,%08x,%08x,%u,%08x,,,,",
                              dut.retire_pc_o, dut.retire_word_o,
                              dut.retire_rd_o, dut.retire_rd_value_o);
                regs[dut.retire_rd_o & 31] = dut.retire_rd_value_o;
            }
            rows.emplace_back(line);
            if (g_verbose) std::printf("row %4zu: %s\n", rows.size() - 1, line);
        }

        if (dut.halt_valid_o) {
            if (dut.halt_word_valid_o)
                std::snprintf(line, sizeof line, "0,%08x,%08x,,,halt:%s,,,",
                              dut.halt_pc_o, dut.halt_word_o,
                              halt_name(dut.halt_reason_o));
            else
                std::snprintf(line, sizeof line, "0,%08x,,,,halt:%s,,,",
                              dut.halt_pc_o, halt_name(dut.halt_reason_o));
            rows.emplace_back(line);
            if (g_verbose) std::printf("halt     : %s\n", line);
            halted = true;
        }

        dut.clk_i = 0;
        dut.eval();
    }

    if (!halted) {
        // House rule 1: a timeout is a failure, not an exit condition. One
        // loud FAIL, remaining checks skipped, nonzero exit. Against the
        // ports-only stub this is the expected, designed red.
        std::printf("FAIL: watchdog expired after %ld cycles without a halt "
                    "(hang detector; healthy run is ~%ld cycles, %zu golden "
                    "rows)\n", watchdog_cycles, expected_cycles, n_rows);
        std::printf("\nSummary: 1 failed out of 1 checks (watchdog)\n");
#if VM_COVERAGE
        Verilated::threadContextp()->coveragep()->write("sim/coverage.dat");
#endif
        return 1;
    }

    std::printf("info: halted after %ld cycles; expected ~%ld, watchdog %ld\n",
                cycle, expected_cycles, watchdog_cycles);

    check(rows.size() == n_rows, "trace row count matches golden");

    size_t compared = rows.size() < n_rows ? rows.size() : n_rows;
    size_t first_bad = compared;
    for (size_t i = 0; i < compared; ++i)
        if (rows[i] != golden_rows[i]) { first_bad = i; break; }
    if (first_bad < compared) {
        std::printf("info: first row mismatch at %zu\n  got      %s\n"
                    "  expected %s\n", first_bad, rows[first_bad].c_str(),
                    golden_rows[first_bad].c_str());
    }
    check(first_bad == compared, "trace rows match golden byte for byte");

    bool regs_ok = true;
    for (int i = 0; i < 32; ++i)
        if (regs[i] != golden_regs[i]) {
            std::printf("info: register mismatch x%d got %08x expected %08x\n",
                        i, regs[i], golden_regs[i]);
            regs_ok = false;
        }
    check(regs_ok, "final registers match golden state");

    bool ram_ok = true;
    for (uint32_t a = 0; a < RAM_BYTES; ++a)
        if (ram[a] != golden_ram[a]) {
            std::printf("info: first RAM mismatch at %08x got %02x expected "
                        "%02x\n", a, ram[a], golden_ram[a]);
            ram_ok = false;
            break;
        }
    check(ram_ok, "final RAM matches golden image");

    check(!rows.empty() &&
              rows.back().find("halt:ebreak") != std::string::npos,
          "program ended in ebreak");

    std::printf("\nSummary: %d failed out of %d checks\n", g_fail_count,
                g_checks_run);
#if VM_COVERAGE
    Verilated::threadContextp()->coveragep()->write("sim/coverage.dat");
#endif
    return g_fail_count ? 1 : 0;
}
