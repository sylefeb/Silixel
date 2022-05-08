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
  vector<t_lut> luts;
  vector<pair<string,int> > outbits;
  vector<int>   ones;
  readDesign(luts, outbits, ones);

  vector<int>   step_starts;
  vector<int>   step_ends;
  vector<uchar> depths;
  analyze(luts, outbits, ones, step_starts, step_ends, depths);

  vector<int>   fanout;
  buildFanout(luts, fanout);

  /// simulate
  vector<uchar> outputs;
  vector<int>   computelists;
  simulInit_cpu(luts, step_starts, step_ends, ones, computelists, outputs);

  LibSL::CppHelpers::Console::clear();

  LibSL::CppHelpers::Console::pushCursor();
  fprintf(stderr, "       _____\n");
  fprintf(stderr, " init_/       ");
  simulPrintOutput_cpu(outputs, outbits);

  LibSL::CppHelpers::Console::popCursor();
  LibSL::CppHelpers::Console::pushCursor();

  int cycles = 0;
  while (1) {

    simulCycle_cpu(luts, depths, step_starts, step_ends, fanout, computelists, outputs);
    simulPosEdge_cpu(luts, depths, (int)step_starts.size(), fanout, computelists, outputs);

    LibSL::CppHelpers::Console::popCursor();
    LibSL::CppHelpers::Console::pushCursor();

    int a = (cycles/3) % 12;
    fprintf(stderr, c_ClockAnim[a * 2 + 0]);
    fprintf(stderr, c_ClockAnim[a * 2 + 1]);
    simulPrintOutput_cpu(outputs, outbits);

    ++cycles;
    // Sleep(500); /// slow down on purpose
  }

	return 0;
}

// -----------------------------------------------------------------------------
