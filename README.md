# Silixel
*Exploring gate-level simulation on CPU and GPU*

> The purpose of this repo is learning about simulation and having fun hacking and understanding how this is possible at all. For actual, efficient simulation please refer to *Verilator*, *CXXRTL* and *Icarus Verilog*.

> **Work in progress**: I am currently working on this README and source code, feedback is welcome.

This repository contains my experiments on gate-level simulation. By that I mean taking the output of [Yosys](https://github.com/YosysHQ/yosys) and simulating the gate network (not taking delays into account - although I believe this could be added).

This all started as I stumbled upon an entry to the Google CTF 2019 contest: [reversing-gpurtl](https://www.youtube.com/watch?v=3ac9HAsfV8c). The source code [is available](https://github.com/google/google-ctf/tree/master/2019/finals/reversing-gpurtl) and shows how to brute force a gate-level simulation onto the GPU.

> What does that mean? How does that work? We're going to precisely answer these questions!

By analyzing the `reversing-gpurtl` source code and scripts (which are in Python and Rust), I got a good understanding of how the gate level simulation was achieved. And I was surprised to discover that it is *simple*!

But first, what is a *gate* in our context? The simplest (and only!) logical element in the network will be a *LUT4*. A LUT (Lookup Up Table) is a basic building block of an FPGA. In my understanding, a simplified LUT4 schematic would look like that:
<center><img src="lut4.png" width="200px"/></center>

The LUT4 has 4 single bit inputs (`a`,`b`,`c`,`d`) and two single bit outputs: `D` and `Q`. Output `D` is 'immediately' updated (as fast as the circuit can do it) when `a`,`b`,`c` or `d` change. `Q` is updated with the value of `D` whenever the clock ticks (positive edge on `clk`). Given `a`,`b`,`c`,`d` the value taken by `D` depends on the LUT configuration, which is a 16 entry truth table (configured by Yosys). It gives the value of bit `D` (0 or 1) based on the values of `a`, `b`, `c` and `d`: four bits that can be either 0 or 1, and thus $2^4=16$ possibilities. This configuration implies that the LUT4 has a small internal memory (16 bits), which is indeed what gets configured by Yosys in the FPGA cells.

Fundamentally, the idea for simulation is as follows:
1. First, ask Yosys to synthesize a design using only LUT4s, see the [script here](synth/synth.yosys).

1. Second, parse the result written by Yosys (a `blif` file) and prepare a data-structure for simulation. The file tells us about the LUT4s and how they are connected. There are a few minor complications that are detailed in the source code comments.

1. Third, run the simulation! The basic idea (we'll improve next) is to simulate all LUTs in parallel. For each LUT, we read its four inputs and update its D output based on its configuration. Once nothing changes, we simulate a positive clock edge by updating the Q output to reflect the value of the D output. Rinse and repeat.

And that's all there is for a basic, working simulator!

Let's now briefly look at an overview of the source code, and then take a closer look at how the simulation behaves. This will lead us to some optimizations, and let us understand the performance tradeoffs.

## Source code overview

To give you a rough outline of the source code:
- Step 1 is covered in the [synth.yosys](synth/synth.yosys) script and [synth.sh](synth.sh).
- Step 2 is covered in [blif.cc](src/blif.cc) and [read.cc](src/read.cc)
- Step 3 is covered in [simul_cpu.cc](src/blif.cc) and [simul_gpu.cc](src/read.cc), both being called from the main app [silixel.cc](src/silixel.cc). A second application does only CPU simulation (see [silixel_cpu.cc](src/silixel_cpu.cc)). It is very simple so that can be a good starting point. The two important GPU shaders are [sh_simul.cs](src/sh_simul.cs) and [sh_posedge.cs](src/sh_posedge.cs).

## A closer look

Blindly simulating all LUTs in parallel works just fine. However, it is quite inefficient in terms of effective *simulated LUT per computation steps*. What do I meant by that?

Let us assume a perfectly parallel computer, with exactly one core per LUT (on a small design and large GPU this might just be the case!).
It turns out that, in most cases, at each 'parallel update' only few LUT outputs are actually changing. This is quite expected: at each simulation step the logic is unlikely to generate changes to all LUTs throughout the entire design. Well, to what extent this is true depends *entirely* on your design of course, but on most designs I tried only a small percentage of LUTs are actually modified.

So what can we do to improve efficiency? We will apply two refinements. The first one
is used both on the CPU and GPU implementations. The second one is used only on the CPU.

### *Refinement 1: sorting LUTs by combinational depth*

Let's have a look at a simple network:

<center><img src="depths.png" width="600px"/></center>

I numbered the LUTs from `L0` to `L5`. The LUTs in the network have been arranged
by *combinational depth*. Given a LUT, the depth counts how many other LUTs are
in between any of its input and a Q (flip-flop) output, *at most*.

> Recall the D outputs are updated as soon as the inputs change (they are *combinational* outputs) while the Q outputs are updated only at the positive clock edge (*registered* outputs).

For instance, `L0` is at depth 0 because both its inputs `a` and `c` read directly
from Q outputs. The same is true of `L1`.
Now `L4` is at depth 1 because while `c` reads from a Q output (which would mean depth 0), `a` reads from the D output of `L1`. Since `L1` is at depth 0, `L4` has to be depth 1. The final depth of the LUT is the largest considering all inputs.

The depth analysis is performed in [analyze.cc](src/analyze.cc).

How does that help? Remember that during simulation, we update all LUTs in parallel
until nothing changes, and then simulate a positive clock edge (Q updated with D).
This introduces two problems. First, we need to track whether something change,
and with large numbers of LUTs that is not free if running parallel on the GPU, for instance.
Second, only few LUT outputs actually change at every iteration, while we update all of them.
In the illustrated example, `L5` would not change until the very last iteration. And during this last
iteration it is the only one to change, so the update is wasted on all other LUTs.

Having the depth gives us some nice properties to reduce the impact of these issues:
- Since we know the maximum overall depth (2 in the example) we know exactly
how many iterations to run and do not have to implement a 'no change' detection.
- LUTs at a same depth are independent from one another (consider `L2`, `L3` and `L4`).
This is true by construction since if one would depend on another, it would have been
assigned at the next depth in the network.
Furthermore, LUTs at a same depth only possibly depend on changes of
the D output of LUTs *at lower depths*. Thus, we can do less work at each iteration,
focusing only on the LUTs that could possibly change. In the example, we would
run three parallel iterations, first {`L0`,`L1`}, then {`L2`,`L3`,`L4`}, then {`L5`}.
This results in substantial savings. On the GPU, we can still update large chunks
of LUTs in parallel *without any synchronization* (LUTs at a same depth), which is
ideal.

> The depth also reflects at what max frequency the circuit can run. Indeed, assuming
it takes the same delay for signal to propagate through all LUTs, the number of LUTs
to traverse *at most* determines the worst case propagation delay, and hence the
maximum frequency.

Now we have seen all the ingredients of the GPU implementation.
See in particular function `simulCycle_gpu` in source file [`simul_gpu.cc`](src/simul_gpu.cc),
that calls the compute shaders on for each depth levels.

> A detail, not discussed here, is that some LUTs remain constant during simulation
and can be skipped. This is done in the implementation.

### *Refinement 2: fanout and compute lists*

The first refinement avoids blind updates to all LUTs. However, it remains
very likely that within a set of LUTs at a same depth, many are updated while
their inputs did not change. Consider {`L2`,`L3`,`L4`}. If only the D output of `L0` changed,
then only `L2` actually requires an update.

This second refinement avoids this issue, implementing a *compute list* per depth level
(including the final positive edge update).
An iteration at a given depth *k* inserts LUTs that should be refreshed in the compute lists of the next depth levels (> *k*).
These are the LUTs using as input the changing D output of a LUT at depth *k*.

To do this efficiently, we first compute the *fanout* of the LUTs. Let us consider a single LUT,
its fanout is the list of LUTs that use its D output (and of course all are deeper in the
network). Given this list, whenever a LUT D output changes we can efficiently add the LUTs of
its fanout to the compute lists
(see `addFanout` in source file [`simul_cpu.cc`](src/simul_cpu.cc)). LUTs are inserted
only once thanks to a 'dirty bit' flag.

This approach works very well on the CPU, which is using a single thread and is
anyway a sequential traversal. In fact, it outperforms the GPU on all but very large
designs (which, on top of it, are large for bad reasons due to memories (BRAM/SPRAM)
being turned into humongous networks of LUTs).

> This approach is not easily amenable to the GPU. I actually tried, but this required
atomic updates, synchronization and indirect compute dispatch ... which in the end
together killed performance. But it might be that I did not find the right way yet!

> Performance can be further improved on the CPU. First, the computations seem a
case for SSE instructions. Second, I should be using more cores! However,
like on the GPU, synchronization can quickly become a performance bottleneck...

## Compile and run

First, make sure to get the submodules:
```
git submodule init
git submodule update
```
Use `CMake` to prepare a Makefile for your system, then `make`:
```
cd build
cmake ..
make
cd ..
```
From a command line in the repo root, run (yosys has to be installed and in PATH):

```
./synth.sh silice_vga_test
```

This synthesizes a design and generate the output in [`build`](./build). There are several designs, see [`designs`](./designs).

After running `silixel` you should see this:

<center><img src="silice_vga_test.gif" width="400px"/></center>
