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

#include <LibSL/LibSL.h>

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iostream>
#include <limits>
#include <algorithm>
#include <string>
#include <sstream>
#include <set>

using namespace std;

#include "read.h"
#include "blif.h"

// -----------------------------------------------------------------------------
/*
  From a read blif file, prepares a data-structure for simulation
  The blif file contains:
  - gates, which are LUT4s
  - latches, which indicate a flip-flop

  In most cases, a latch corresponds to the output of a gate, so it is
  simply a matter of connecting to the Q output of the corresponding gate.
  There are cases however where latches are chained. This requires us to
  instantiate gates to implement the flip-flops (since we only simulate
  gates). These gates are passthrough, and only their Q output is used,
  see tag [extra gates] in comments below.
*/
void buildSimulData(
  t_blif&                     _blif, // might change (adding extra LUTs)
  vector<t_lut>&              _luts, // output vector of LUTs (see header)
  vector<pair<string, int> >& _outbits, // output bit indices
  vector<int>&                _ones) // which output bit start as '1'
{
  // gather output names and their source gate/latch
  map<string, v2i> output2src;
  ForArray(_blif.gates, g) {
    output2src[_blif.gates[g].output] = v2i(0, g);
  }
  ForArray(_blif.latches, l) {
    output2src[_blif.latches[l].output] = v2i(1, l);
  }
  // number all outputs
  map<string, int> indices;
  // prepare to create luts
  vector<int> lut_gates;
  // -> find register outputs that depend on other registers
  for (const auto& o : output2src) {
    if (o.second[0]) { // latch
      // find input type
      sl_assert(output2src.count(_blif.latches[o.second[1]].input));
      const auto& I = output2src.find(_blif.latches[o.second[1]].input);
      if (I->second[0]) {
        // input of this latch is the output (Q) of an earlier latch
        // we need a pass-through gate to do that      [extra gates]
        int g = (int)_blif.gates.size();
        _blif.gates.push_back(t_gate_nfo());
        _blif.gates.back().config_strings.push_back(make_pair("1", "1"));
        _blif.gates.back().inputs.push_back(I->first);
        string ex = "__extra__" + I->first;
        _blif.gates.back().output = ex;
        _blif.latches[o.second[1]].input = ex;
        output2src[ex] = v2i(0, g);
      }
    }
  }
  // -> create one LUT per latch
  for (const auto& o : output2src) {
    if (o.second[0]) { // latch
      // find input type
      const auto& I = output2src.find (_blif.latches[o.second[1]].input);
      sl_assert(I != output2src.end());
      sl_assert(I->second[0] == 0); // other has to be a gate
      /// create LUT for the D output (latch input)
      /// assign output to Q (latch output)
      // store indices of input (D) and output (Q)
      indices[I->first] = (((int)lut_gates.size()) << 1);
      indices[o.first]  = (((int)lut_gates.size()) << 1) + 1;
      // gate that corresponds to the lut
      lut_gates.push_back(I->second[1]);
    }
  }
  // -> create one LUT per comb output
  for (const auto& o : output2src) {
    if (!o.second[0]) { // gate
      // ignore clock
      if (o.first == "clock") {
        continue;
      }
      // check if the output is already assigned
      if (indices.count(o.first)) {
        continue;
      }
      // check that it does not use clock // NOTE:investigate
      bool skip = false;
      for (auto i : _blif.gates[o.second[1]].inputs) {
        if (i == "clock") {
          skip = true;
          break;
        }
      }
      if (skip) continue;
      /// create LUT for the D output
      // store index
      indices[o.first] = (((int)lut_gates.size()) << 1);
      // gate that corresponds to the lut
      lut_gates.push_back(o.second[1]);
    }
  }
  // -> instantiate LUTs
  for (auto g : lut_gates) {
    _luts.push_back(t_lut());
    _luts.back().cfg = lut_config(_blif.gates[g].config_strings);
    ForIndex(i, 4) {
      _luts.back().inputs[i] = -1;
    }
    int i = 4 - (int)_blif.gates[g].inputs.size();
    for (auto inp : _blif.gates[g].inputs) {
      auto I = indices.find(inp);
      if (I == indices.end()) {
        fprintf(stderr, "<warning> input '%s' disconnected\n",inp.c_str());
      } else {
        _luts.back().inputs[i++] = I->second;
      }
    }
  }

  for (auto op : _blif.outputs) {
    auto I = indices.find(op);
    if (I == indices.end()) {
      fprintf(stderr, "<warning> outport '%s' disconnected\n", op.c_str());
    } else {
      _outbits.push_back(make_pair(op, I->second));
    }
  }

  for (const auto& l : _blif.latches) {
    if (l.init == "1") {
      auto I = indices.find(l.output);
      sl_assert(I != indices.end());
      _ones.push_back(I->second);
    }
  }

  /// DEBUG
#if 0
  for (int l = 0; l < _luts.size(); ++l) {
    fprintf(stderr,"LUT %3d, cfg:%4x, inputs: %4d %4d %4d %4d\n",
      l<<1,_luts[l].cfg,
      _luts[l].inputs[0], _luts[l].inputs[1],
      _luts[l].inputs[2], _luts[l].inputs[3]);
  }
#endif
}

// -----------------------------------------------------------------------------

/*
  Reads the design from a BLIF file
*/
void readDesign(
  vector<t_lut>& _luts,
  vector<pair<string, int> >& _outbits,
  vector<int>& _ones)
{
  t_blif blif;
  // parse the blif file
  parse(SRC_PATH "/build/synth.blif", blif);
  // build the design datastructure
  buildSimulData(blif, _luts, _outbits, _ones);
}

// -----------------------------------------------------------------------------
