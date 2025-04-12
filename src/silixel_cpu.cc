// @sylefeb 2022-01-04
/*
BSD 3-Clause License

Copyright (c) 2022, Sylvain Lefebvre (@sylefeb)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// --------------------------------------------------------------

#include <LibSL.h>

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iostream>
#include <limits>
#include <algorithm>
#include <string>

using namespace std;

#include "simul_cpu.h"
#include "fstapi/fstapi.h"
#define FST_TS_S    0
#define FST_TS_MS  -3
#define FST_TS_US  -6
#define FST_TS_NS  -9
#define FST_TS_PS  -12

// -----------------------------------------------------------------------------

typedef struct {
  string    name;
  int       index;
  fstHandle fst;
} t_watch;

void add_watch(string signal, const map<string, int>& indices, vector<t_watch> &_watches)
{
  auto I = indices.find(signal);
  if (I == indices.end()) {
    fprintf(stderr, "<error> cannot find signal '%s' to watch\n", signal.c_str());
    exit (-1);
  }
  t_watch w;
  w.name = signal;
  w.index = I->second;
  w.fst = 0;
  _watches.push_back(w);
}

// -----------------------------------------------------------------------------

const char *c_ClockAnim[] = {
"       _____  \n",
" _____/     \\  ",
"      _____   \n",
" ____/     \\_  ",
"     _____    \n",
" ___/     \\__  ",
"    _____     \n",
" __/     \\___  ",
"   _____      \n",
" _/     \\____  ",
"  _____       \n",
" /     \\_____  ",
" _____        \n",
"      \\_____/  ",
" ____       _ \n",
"     \\_____/   ",
" ___       __ \n",
"    \\_____/    ",
" __       ___ \n",
"   \\_____/     ",
" _       ____ \n",
"  \\_____/      ",
"        _____ \n",
" \\_____/       ",
};


int main(int argc,char **argv)
{
  /// load up design
  vector<t_lut>             luts;
  std::vector<t_bram>       brams;
  vector<pair<string,int> > outbits;
  vector<int>               ones;
  map<string, int>          indices;
  readDesign(luts, brams, outbits, ones, indices);

  vector<int>   step_starts;
  vector<int>   step_ends;
  vector<uchar> depths;
  analyze(luts, brams, outbits, indices, ones, step_starts, step_ends, depths);

  vector<int>   fanout;
  buildFanout(luts, fanout);

  /// add reset to init to ones
  bool has_reset = indices.count("reset") > 0;
  if (has_reset) {
    ones.push_back(indices.at("reset"));
  }

  /// simulate
  vector<uchar> outputs;
  vector<int>   computelists;
  simulInit_cpu(luts, brams, step_starts, step_ends, ones, computelists, outputs);

  vector<t_watch> watches;
  //ForIndex(i, 3) {
  // add_watch("_q__idx_fsm0[" + std::to_string(i) + "]", indices, watches);
  //}
  if (has_reset) {
    add_watch("reset", indices, watches);
  }

  // LibSL::CppHelpers::Console::clear();

  // LibSL::CppHelpers::Console::pushCursor();
  fprintf(stderr, "       _____\n");
  fprintf(stderr, " init_/       ");
  simulPrintOutput_cpu(outputs, outbits);

  // print watches
  for (auto w : watches) {
    int b        = w.index;
    int lut      = b >> 1;
    int q_else_d = b & 1;
    int bit      = (outputs[lut] >> q_else_d) & 1;
    fprintf(stderr, "(%d) %s\t%d\n", b, w.name.c_str(), bit);
  }

  // FST trace
  fstWriterContext *fst = fstWriterCreate("trace.fst", 1);
  fstWriterSetTimescale(fst, 1);
  fstWriterSetScope(fst, FST_ST_VCD_MODULE, "top", NULL);
  for (auto& w : watches) {
    w.fst = fstWriterCreateVar(fst, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, w.name.c_str(), NULL);
  }

  // LibSL::CppHelpers::Console::popCursor();
  // LibSL::CppHelpers::Console::pushCursor();

  int cycles = 0;
  // while (1) {
  while (cycles < 100) {

    if (has_reset) {
      if (cycles < 16) {
        simulSetSignal_cpu(indices.at("reset"), true, depths, (int)step_starts.size(), fanout, computelists, outputs);
      } else if (cycles == 16) {
        simulSetSignal_cpu(indices.at("reset"), false, depths, (int)step_starts.size(), fanout, computelists, outputs);
      }
    }

    simulCycle_cpu(luts, brams, depths, step_starts, step_ends, fanout, computelists, outputs);
    simulPosEdge_cpu(luts, depths, (int)step_starts.size(), fanout, computelists, outputs);

//    LibSL::CppHelpers::Console::popCursor();
//    LibSL::CppHelpers::Console::pushCursor();

    int a = (cycles/3) % 12;
    fprintf(stderr, c_ClockAnim[a * 2 + 0]);
    fprintf(stderr, c_ClockAnim[a * 2 + 1]);
    simulPrintOutput_cpu(outputs, outbits);

    // print and trace watches
    fstWriterEmitTimeChange(fst, cycles);
    for (auto w : watches) {
      int b        = w.index;
      int lut      = b >> 1;
      int q_else_d = b & 1;
      int bit      = (outputs[lut] >> q_else_d) & 1;
      fprintf(stderr, "(%d) %s\t%d\n", b,w.name.c_str(), bit);
      fstWriterEmitValueChange(fst, w.fst, bit ? "1" : "0");
    }

    ++cycles;
    // Sleep(500); /// slow down on purpose
  }

  fstWriterClose(fst);

	return 0;
}

// -----------------------------------------------------------------------------
