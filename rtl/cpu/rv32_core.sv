// rv32_core: M1 harness stub, ports only (docs/cpu-contract.md 4c, D23).
//
// Deliberately empty. tb/sim_cpu.cpp must be seen red against this file
// before any real core logic lands (house rule 4 lifted to the harness:
// a tb that cannot fail against nothing is nothing). M1 replaces this
// body and retires this header comment with it.

/* verilator lint_off UNUSEDPARAM */
module rv32_core #(
    parameter int POR_CYCLES = 2**24, // board value; sim overrides via -G
    parameter string FIRMWARE0 = "tools/golden/hart0.hex"
) (
    input  logic        clk_i,          // clk_50m domain
    input  logic        rst_n_i,        // active low, deassertion synced

    // Retire tap: contract section 9 row source, at most one per clock.
    output logic        retire_valid_o,
    output logic [31:0] retire_pc_o,
    output logic [31:0] retire_word_o,
    output logic [4:0]  retire_rd_o,
    output logic [31:0] retire_rd_value_o,
    output logic        retire_mem_valid_o,
    output logic        retire_mem_store_o,
    output logic        retire_mem_sign_o,  // loads only
    output logic [2:0]  retire_mem_size_o,  // bytes: 1, 2, 4
    output logic [31:0] retire_mem_addr_o,
    output logic [31:0] retire_mem_value_o, // load: raw right-justified;
                                            // store: lane-placed word

    // Halt tap: latching, terminal. reason: 0=ebreak 1=ecall 2=illegal
    // 3=bus. Budget is an ISS artifact; the harness watchdog covers hangs.
    output logic        halt_valid_o,
    output logic [1:0]  halt_reason_o,
    output logic [31:0] halt_pc_o,
    output logic        halt_word_valid_o,
    output logic [31:0] halt_word_o
);

    assign retire_valid_o     = 1'b0;
    assign retire_pc_o        = '0;
    assign retire_word_o      = '0;
    assign retire_rd_o        = '0;
    assign retire_rd_value_o  = '0;
    assign retire_mem_valid_o = 1'b0;
    assign retire_mem_store_o = 1'b0;
    assign retire_mem_sign_o  = 1'b0;
    assign retire_mem_size_o  = '0;
    assign retire_mem_addr_o  = '0;
    assign retire_mem_value_o = '0;

    assign halt_valid_o      = 1'b0;
    assign halt_reason_o     = '0;
    assign halt_pc_o         = '0;
    assign halt_word_valid_o = 1'b0;
    assign halt_word_o       = '0;

    logic _unused_ok = &{1'b0, clk_i, rst_n_i};

endmodule
/* verilator lint_on UNUSEDPARAM */
