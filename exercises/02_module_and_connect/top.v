/**/
`timescale 1ns / 1ps
`default_nettype none


module top (
    input  wire wire_a,
    input  wire wire_b,
    output wire wire_inverter,
    output wire wire_and_gate,
    output wire wire_or_gate,
    output wire wire_xor_gate,
    output wire wire_pass_through,
    output wire wire_inverter_2
);

  wire mid;
  pass_through u_pass (
      .a(wire_a),
      .c(wire_pass_through)
  );

  inverter u_inverter (
      .a(wire_a),
      .c(wire_inverter)
  );

  and_gate U_and_gate (
      .a(wire_a),
      .b(wire_b),
      .c(wire_and_gate)
  );

  or_xor_gate u_or_xor_gate (
      .a(wire_a),
      .b(wire_b),
      .c(wire_or_gate),
      .d(wire_xor_gate)
  );

  inverter u_inverter_2_1 (
      .a(wire_a),
      .c(mid)
  );

  inverter u_inverter_2_2 (
      .a(mid),
      .c(wire_inverter_2)
  );
endmodule

`default_nettype wire
