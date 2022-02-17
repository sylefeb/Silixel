#!/bin/bash

# run ./make.sh DESIGN
# (without the .v, where design is a Verilog file in designs)

# rm build/*
mkdir build

cp designs/$1.v build/synth.v

cd synth
yosys -s synth.yosys
cd ..
