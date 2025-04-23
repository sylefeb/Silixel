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
#include "blif.h"
#include "fstapi/fstapi.h"
#define FST_TS_S    0
#define FST_TS_MS  -3
#define FST_TS_US  -6
#define FST_TS_NS  -9
#define FST_TS_PS  -12

// -----------------------------------------------------------------------------

typedef struct {
  string     name;
  string     base_name;
  int        bit_index;
  int        lut_index;
  fstHandle  fst_handle;
  fstVarType fst_type;
} t_watch;

string base_name(string str)
{
  size_t dot_pos = str.rfind('.');
  size_t pos = str.find('[');
  if (pos != std::string::npos) {
    if (dot_pos != std::string::npos) {
      return str.substr(dot_pos + 1, pos - dot_pos - 1);
    } else {
      return str.substr(0, pos);
    }
  } else if (dot_pos != std::string::npos) {
    return str.substr(dot_pos+1);
  } else {
    return str;
  }
}

int index(string str)
{
  size_t s = str.find('[');
  size_t e = str.find(']', s);
  if (s == std::string::npos || e == std::string::npos || s >= e - 1) {
    return -1; // no index
  }
  string istr = str.substr(s + 1, e - s - 1);
  return std::stoi(istr);
}

t_watch& add_watch(string signal, const map<string, int>& indices, vector<t_watch> &_watches)
{
  auto I = indices.find(signal);
  if (I == indices.end()) {
    fprintf(stderr, "<error> cannot find signal '%s' to watch\n", signal.c_str());
    exit (-1);
  }
  t_watch w;
  w.name = signal;
  w.base_name = base_name(w.name);
  w.bit_index = index(w.name);
  w.lut_index = I->second;
  w.fst_handle = 0;
  w.fst_type = FST_VT_VCD_WIRE;
  _watches.push_back(w);
  return _watches.back();
}

void setFstScope(fstWriterContext *fst, string signal)
{
  vector<string> path;
  split(signal, '.', path);
  if (!path.empty()) path.pop_back();
  for (auto node : path) {
    fstWriterSetScope(fst, FST_ST_VCD_MODULE, node.c_str(), NULL);
  }
}

void unsetFstScope(fstWriterContext *fst, string signal)
{
  vector<string> path;
  split(signal, '.', path);
  if (!path.empty()) path.pop_back();
  for (auto node : path) {
    fstWriterSetUpscope(fst);
  }
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


int main(int argc,const char **argv)
{
  bool silice_design = false;
  int num_cycles = 10000;
  const char *blif_path = SRC_PATH "/build/synth.blif";

  fprintf(stderr, "<<<====----- Silixel v0.1 by @sylefeb -----====>>>\n");

  /// parse options
  int i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "--silice") == 0) {
      silice_design = true;
      ++i;
    } else if (strcmp(argv[i], "--cycles") == 0) {
      if (i + 1 == argc) {
        fprintf(stderr, "--cycles expects a parameter (integer, number of cycles to simulate)\n");
        exit(-1);
      }
      ++i;
      num_cycles = atoi(argv[i]);
      ++i;
    } else if (strcmp(argv[i], "--blif") == 0) {
      if (i + 1 == argc) {
        fprintf(stderr, "--blif expects a parameter (string, file to load)\n");
        exit(-1);
      }
      ++i;
      blif_path = argv[i];
      ++i;
    } else { ++i; }
  }

  /// checks
  {
    FILE *f = 0;
    fopen_s(&f, blif_path, "rb");
    if (f == NULL) {
      fprintf(stderr, "<error> cannot open input blif file %s\n", blif_path);
      exit(-1);
    } else {
      fclose(f);
    }
  }

  /// load up design
  vector<t_lut>             luts;
  std::vector<t_bram>       brams;
  vector<pair<string,int> > outbits;
  vector<int>               ones;
  map<string, int>          indices;
  readDesign(blif_path, luts, brams, outbits, ones, indices);

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

  /// automatically add all outputs as watches
  vector<t_watch> watches;
  if (silice_design) {
    // selection specialized to a silice design
    for (auto signal : indices) {
      if (signal.first.substr(0, 3) == "out") {
        auto &w = add_watch(signal.first, indices, watches);
        w.fst_type = FST_VT_VCD_WIRE;
      } else if (signal.first.find("_q_") != std::string::npos) {
        auto &w = add_watch(signal.first, indices, watches);
        w.fst_type = FST_VT_VCD_REG;
      }
    }
  } else {
    // selection for any other design
    for (auto signal : indices) {
      if (signal.first[0] != '$') {
        auto &w = add_watch(signal.first, indices, watches);
        w.fst_type = FST_VT_VCD_WIRE;
      }
    }
  }
  if (has_reset) {
    add_watch("reset", indices, watches);
  }

  LibSL::CppHelpers::Console::clear();
  LibSL::CppHelpers::Console::pushCursor();
  fprintf(stderr, "       _____\n");
  fprintf(stderr, " init_/       ");
  // simulPrintOutput_cpu(outputs, outbits);

  // FST trace
  fstWriterContext *fst = fstWriterCreate("./trace.fst", 1);
  if (fst == NULL) {
    fprintf(stderr,"cannot open trace.fst for writing\n");
    exit (-1);
  }
  fstWriterSetTimescale(fst, 1);
  fstWriterSetScope(fst, FST_ST_VCD_MODULE, "top", NULL);
  // -> group individual bits
  map<string, int> bitcounts;
  for (auto &w : watches) {
    bitcounts[w.base_name] = max(bitcounts[w.base_name], index(w.name)+1);
  }
  set<string> added;
  for (auto& w : watches) {
    if (!added.count(w.base_name)) {
      added.insert(w.base_name);
      setFstScope(fst, w.name);
      w.fst_handle = fstWriterCreateVar(fst, FST_VT_VCD_REG, FST_VD_IMPLICIT, max(1,bitcounts[w.base_name]), w.base_name.c_str(), NULL);
      unsetFstScope(fst, w.name);
    }
  }
  auto fst_clock = fstWriterCreateVar(fst, FST_VT_VCD_REG, FST_VD_IMPLICIT, 1, "clock", NULL);

  LibSL::CppHelpers::Console::popCursor();
  LibSL::CppHelpers::Console::pushCursor();

  int anim = 0;
  Every ev(100);

  int cycles = 0;
  while (num_cycles == -1 || cycles < num_cycles) {

    if (has_reset) {
      if (cycles < 16) {
        simulSetSignal_cpu(indices.at("reset"), true, depths, (int)step_starts.size(), fanout, computelists, outputs);
      } else if (cycles == 16) {
        simulSetSignal_cpu(indices.at("reset"), false, depths, (int)step_starts.size(), fanout, computelists, outputs);
      }
    }

    fstWriterEmitTimeChange(fst, (cycles << 1) + 0);
    fstWriterEmitValueChange(fst, fst_clock, "0");
    simulCycle_cpu(luts, brams, depths, step_starts, step_ends, fanout, computelists, outputs);

    fstWriterEmitTimeChange(fst, (cycles << 1) + 1);
    fstWriterEmitValueChange(fst, fst_clock, "1");
    simulPosEdge_cpu(luts, depths, (int)step_starts.size(), fanout, computelists, outputs);

    int console_out = ev.expired();

    if (console_out) {
      LibSL::CppHelpers::Console::popCursor();
      LibSL::CppHelpers::Console::pushCursor();
      int a = anim % 12;
      fprintf(stderr, c_ClockAnim[a * 2 + 0]);
      fprintf(stderr, c_ClockAnim[a * 2 + 1]);
      ++anim;
      if (num_cycles > -1) {
        fprintf(stderr, " (%7d cycles, %3d%% completed)\n", cycles, 100 * cycles / num_cycles);
      } else {
        fprintf(stderr, "\n");
      }
    }

    // print and trace watches
    map<string, string> values;
    for (auto w : watches) {
      int b        = w.lut_index;
      int lut      = b >> 1;
      int q_else_d = b & 1;
      int bit      = (outputs[lut] >> q_else_d) & 1;
      if (w.bit_index > -1) {
        if (values[w.base_name].empty()) {
          values[w.base_name].resize(bitcounts[w.base_name], '0');
        }
        values[w.base_name][bitcounts[w.base_name]-1-w.bit_index] = bit ? '1' : '0';
      } else {
        values[w.base_name] = bit ? "1" : "0";
      }
    }
    set<string> added;
    const int max_display = 16;
    int num_display = 0;
    for (auto w : watches) {
      if (!added.count(w.base_name)) {
        added.insert(w.base_name);
        fstWriterEmitValueChange(fst, w.fst_handle, values[w.base_name].c_str());
        if (console_out && num_display < max_display) {
          fprintf(stderr, "%-40s %s\n", w.base_name.c_str(), values[w.base_name].c_str());
          ++num_display;
        }
      }
    }

    ++cycles;
    // Sleep(500); /// slow down on purpose
  }

  fstWriterClose(fst);

  fprintf(stderr, "\n\noutput: trace.fst\n\n");

	return 0;
}

// -----------------------------------------------------------------------------
