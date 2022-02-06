# Silixel
*Exploring gate-level simulation on CPU and GPU*

This repository contains my experiments on gate-level simulation. By that I mean taking the output of [Yosys](https://github.com/YosysHQ/yosys) and simulating the gate network.

This all started as I stumbled upon an entry to the Google CTF 2019 contest: [reversing-gpurtl](https://www.youtube.com/watch?v=3ac9HAsfV8c). The source code [is available](https://github.com/google/google-ctf/tree/master/2019/finals/reversing-gpurtl) and shows how to brute force a gate-level simulation onto the GPU.

What does that mean? How does that work? We're going to precisely answer these questions!

By analyzing the `reversing-gpurtl` source code and scripts (which are in Python and Rust), I got a good understanding of how the gate level simulation was achieved. And I was surprised to discover that it is *simple*!

Fundamentally, the idea is as follows:
1. Ask Yosys to synthesize a design using only LUT4s. A LUT (Lookup Up Table) is a basic building block of an FPGA, a LUT4 schematic looks like that:<center><img src="lut4.png" width="200px"/></center> The LUT4 has 4 inputs (a,b,c,d) and two outputs: D and Q. Output D is 'immediately' updated (as fast as the circuit can do it) when a,b,c or d change. Q is updated with the value of D whenever the clock ticks (positive edge). Given a,b,c,d the value taken by D depends on the LUT configuration, which is 16 entry truth table (configured by Yosys).
1. Read the result and prepare a data-structure for simulation.
1. Run simulation: simulate all LUTs in parallel. For each, read its four inputs and update its D output based on its configuration. On a positive clock edge, update the Q output to reflect the value of the D output.

And that's all there is to it. Now of course there are some implementations details.


## Objectives

- Learn how a simple gate level simulator operates
- Provide both a CPU and GPU implementation
- Simulate non trivial designs
- Be a starting point for other, more specialized projects
