# Silixel
*Exploring gate-level simulation on CPU and GPU*

> **Note:** The purpose of this repo is learning about simulation and having fun hacking and understanding how this is possible at all. For actual simulation please refer to *Verilator*, *CXXRTL* and *Icarus Verilog*.

This repository contains my experiments on gate-level simulation. By that I mean taking the output of [Yosys](https://github.com/YosysHQ/yosys) and simulating the gate network (not taking delays into account - although I believe this could be added).

This all started as I stumbled upon an entry to the Google CTF 2019 contest: [reversing-gpurtl](https://www.youtube.com/watch?v=3ac9HAsfV8c). The source code [is available](https://github.com/google/google-ctf/tree/master/2019/finals/reversing-gpurtl) and shows how to brute force a gate-level simulation onto the GPU.

> What does that mean? How does that work? We're going to precisely answer these questions!

By analyzing the `reversing-gpurtl` source code and scripts (which are in Python and Rust), I got a good understanding of how the gate level simulation was achieved. And I was surprised to discover that it is *simple*!

Fundamentally, the idea is as follows:
1. First, ask Yosys to synthesize a design using only LUT4s, see the [script here](synth/synth.yosys). A LUT (Lookup Up Table) is a basic building block of an FPGA. In my understanding, a simplified LUT4 schematic would look like that:<center><img src="lut4.png" width="200px"/></center> The LUT4 has 4 single bit inputs (`a`,`b`,`c`,`d`) and two single bit outputs: `D` and `Q`. Output `D` is 'immediately' updated (as fast as the circuit can do it) when `a`,`b`,`c` or `d` change. `Q` is updated with the value of `D` whenever the clock ticks (positive edge on `clk`). Given `a`,`b`,`c`,`d` the value taken by `D` depends on the LUT configuration, which is a 16 entry truth table (configured by Yosys). It gives the value of bit `D` (0 or 1) based on the values of `a`, `b`, `c` and `d`: four bits that can be either 0 or 1, and thus $2^4=16$ possibilities. This configuration implies that the LUT4 has a small internal memory (16 bits), which is indeed what gets configured by Yosys in the FPGA cells.

1. Second, parse the result written by Yosys (a `blif` file) and prepare a data-structure for simulation. The file tells us about the LUT4s and how they are connected. There are a few minor complications that are detailed in the source code comments.

1. Third, run the simulation! The basic idea (we'll improve next) is to simulate  all LUTs in parallel. For each LUT, we read its four inputs and update its D output based on its configuration. Once nothing changes, we simulate a positive clock edge by updating the Q output to reflect the value of the D output. Rinse and repeat.

And that's all there is to it for a basic, working simulator!

To give you a rough outline of the source code:
- Step 1 is covered in the [synth.yosys](synth/synth.yosys) script and [synth.sh](synth.sh).
- Step 2 is covered in [blif.cc](src/blif.cc) and [read.cc](src/read.cc)
- Step 3 is covered in [simul_cpu.cc](src/blif.cc) and [simul_gpu.cc](src/read.cc), both being called from the main app [silixel.cc](src/silixel.cc). A second app does only CPU simulation -- [silixel_cpu.cc](src/silixel_cpu.cc) -- it is very simple so that can be a good starting point. The two important GPU shaders are [sh_simul.cs](src/sh_simul.cs) and [sh_posedge.cs](src/sh_posedge.cs).

## Compile and run

First, make sure to get the submodules:
```
git submodule init
git submodule update
```
Use `CMake` to prepare a Makefile for your system, then `make`.
From a command line in the repo root, run (yosys has to be installed and in PATH):

```
./synth.sh silice_vga_test
```

This synthesizes a design and generate the output in [`build`](./build). There are several designs, see [`designs`](./designs).
