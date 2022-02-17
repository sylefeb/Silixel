// @sylefeb 2022-01-04
// --------------------------------------------------------------

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
#include <LibSL.h>

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
