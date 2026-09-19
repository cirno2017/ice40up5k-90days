位反转写法，适用于任何位宽
module bit_reverse #(
    parameter WIDTH = 8
) (
    input  wire [WIDTH-1:0] din,
    output wire [WIDTH-1:0] dout
);

    genvar i;
    generate
        for (i = 0; i < WIDTH; i = i + 1) begin : gen_reverse
            assign dout[i] = din[WIDTH-1-i];
        end
    endgenerate

endmodule
实例化bit_reverse #(.WIDTH(8)) u_reverse (
    .din  (switches),
    .dout (leds2)
);
显式切片是闭区间包括两端
截断是丢弃高位保留低位