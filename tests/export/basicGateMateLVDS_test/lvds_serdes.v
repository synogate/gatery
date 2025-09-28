module ARCH_GM_SERDES14 
    #(parameter COL = 110)
  (input  clk,
   input  clk7x,
   input  [13:0] in_data,
   input  in_sync,
   output out_bit);

    wire [13:0] data_shift_reg;
    wire real_sync;

    //(* keep, BEL=$sformatf("X%0dY%0d/CPE_FF_%s", COL, 11, "L") *)
    CC_DFF SyncReg (
        .CLK(clk7x),
        .EN(1'b1),
        .SR(1'b0),
        .D(in_sync),
        .Q(real_sync)
    );

    genvar i;
    generate 
        for (i = 0; i < 14; i = i + 1) begin : bitelement

            //wire pre_buffer;
            //(* keep, BEL=$sformatf("X%0dY%0d/CPE_HALF_%s", COL-1, 4 + i / 2, (i % 2 == 0) ? "L" : "U") *)
            //CC_DFF DataSynchronizerIn (
            //    .CLK(clk),
            //    .EN(1'b1),
            //    .SR(1'b0),
            //    .D(in_data[i]),
            //    .Q(pre_buffer));

            wire in_buffer;
            //(* keep, BEL=$sformatf("X%0dY%0d/CPE_FF_%s", COL, 4 + i / 2, (i % 2 == 0) ? "L" : "U") *)
            CC_DFF DataSynchronizerOut (
                .CLK(clk7x),
                .EN(real_sync),
                .SR(1'b0),
                .D(in_data[i]),
                .Q(in_buffer));

            wire selected;

            if(i > 11) begin
                assign selected = in_buffer;
            end else begin
                //(* keep, BEL=$sformatf("X%0dY%0d/CPE_HALF_%s", COL+1, 4 + i / 2, (i % 2 == 0) ? "L" : "U") *)
                CC_MX2 Loader (.D0(data_shift_reg[i + 2]), .D1(in_buffer), .S0(real_sync), .Y(selected));
            end

            //(* keep, BEL=$sformatf("X%0dY%0d/CPE_FF_%s", COL+1, 4 + i / 2, (i % 2 == 0) ? "L" : "U") *)
            CC_DFF ShiftReg (
                .CLK(clk7x),
                .EN(1'b1),
                .SR(1'b0),
                .D(selected),
                .Q(data_shift_reg[i])
            );

        end
    endgenerate

    CC_ODDR cc_oddr_inst (
        .CLK(clk7x),
        .DDR(clk7x),
        .D0(data_shift_reg[1]),
        .D1(data_shift_reg[0]),
        .Q(out_bit)
    );

endmodule

module ARCH_GM_OPENLDI(
    input clk,
    input clk7x,
    input [0:13] in_O0,
    input [0:13] in_O1,
    input [0:13] in_O2,
    input [0:13] in_O3,
    input [0:13] in_E0,
    input [0:13] in_E1,
    input [0:13] in_E2,
    input [0:13] in_E3,

    output out_O0,
    output out_O1,
    output out_O2,
    output out_O3,
    output out_OC,
    output out_E0,
    output out_E1,
    output out_E2,
    output out_E3,
    output out_EC,
);
`ifdef 0
    wire trigger;
    wire [0:4] sync_reg;

    (* KEEP *)
    CC_DFF TriggerReg (
        .CLK(clk),
        .EN(1'b1),
        .SR(1'b0),
        .D(~trigger),
        .Q(trigger)
    );

    genvar i;
    generate 
        for (i = 0; i < 5; i = i + 1) begin : sync_chain
            (* KEEP *)
            CC_DFF SyncReg0 (
                .CLK(clk7x),
                .EN(1'b1),
                .SR(1'b0),
                .D((i == 0) ? trigger : sync_reg[i-1]),
                .Q(sync_reg[i])
            );
        end
    endgenerate

    wire sync;
    (* KEEP *)
    CC_DFF SyncReg (
        .CLK(clk7x),
        .EN(1'b1),
        .SR(1'b0),
        .D(sync_reg[1] ^ sync_reg[2]),
        .Q(sync)
    );
`else

    wire enableCounter;
    (* keep *)
    CC_DFF #(.INIT(1'b0)) TriggerReg (
        .CLK(clk),
        .EN(1'b1),
        .SR(1'b0),
        .D(1'b1),
        .Q(enableCounter)
    );


    reg sync;
    reg [0:2] ctr;
    always @(posedge clk7x)
    begin
        ctr <= ctr + enableCounter;
        if(ctr == 6) begin
            ctr <= 0;
        end
        sync <= (ctr == 1);
    end
`endif


    ARCH_GM_SERDES14 #(.COL(80)) inst_E0 (.clk(clk), .clk7x(clk7x), .in_data(in_E0), .in_sync(sync), .out_bit(out_E0));
    ARCH_GM_SERDES14 #(.COL(84)) inst_E1 (.clk(clk), .clk7x(clk7x), .in_data(in_E1), .in_sync(sync), .out_bit(out_E1));
    ARCH_GM_SERDES14 #(.COL(88)) inst_E2 (.clk(clk), .clk7x(clk7x), .in_data(in_E2), .in_sync(sync), .out_bit(out_E2));
    ARCH_GM_SERDES14 #(.COL(92)) inst_E3 (.clk(clk), .clk7x(clk7x), .in_data(in_E3), .in_sync(sync), .out_bit(out_E3));
    ARCH_GM_SERDES14 #(.COL(96)) inst_EC (.clk(clk), .clk7x(clk7x), .in_data(14'b00111000011100), .in_sync(sync), .out_bit(out_EC));
    ARCH_GM_SERDES14 #(.COL(100)) inst_O0 (.clk(clk), .clk7x(clk7x), .in_data(in_O0), .in_sync(sync), .out_bit(out_O0));
    ARCH_GM_SERDES14 #(.COL(104)) inst_O1 (.clk(clk), .clk7x(clk7x), .in_data(in_O1), .in_sync(sync), .out_bit(out_O1));
    ARCH_GM_SERDES14 #(.COL(108)) inst_O2 (.clk(clk), .clk7x(clk7x), .in_data(in_O2), .in_sync(sync), .out_bit(out_O2));
    ARCH_GM_SERDES14 #(.COL(112)) inst_O3 (.clk(clk), .clk7x(clk7x), .in_data(in_O3), .in_sync(sync), .out_bit(out_O3));
    ARCH_GM_SERDES14 #(.COL(116)) inst_OC (.clk(clk), .clk7x(clk7x), .in_data(14'b00111000011100), .in_sync(sync), .out_bit(out_OC));

endmodule
