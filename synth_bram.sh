#!/bin/bash

# run ./synth.sh DESIGN
# (without the .v, where design is a Verilog file in ./designs/)

mkdir build

cp designs/$1.v build/synth.v

cd synth
yosys -s synth_bram.yosys
cd ..
