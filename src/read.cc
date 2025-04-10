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
  t_blif&                     _blif,    // might change (adding extra LUTs)
  vector<t_lut>&              _luts,    // output vector of LUTs (see header)
  vector<t_bram>&             _brams,   // output vector of BRAMs
  vector<pair<string, int> >& _outbits, // output bit indices
  vector<int>&                _ones,    // which output bit start as '1'
  map<string, int>&           _indices) // signal to LUT map
{
  // gather output names and their source gate/latch
  map<string, v2i> output2src;
  // -> gates
  ForArray(_blif.gates, g) {
    output2src[_blif.gates[g].output] = v2i(0, g);
  }
  // -> latches
  ForArray(_blif.latches, l) {
    output2src[_blif.latches[l].output] = v2i(1, l);
  }
  // -> BRAMs
  ForArray(_blif.brams, b) {
    for (int i=0; i < _blif.brams[b].data_width; ++i) {
      string port = "RD_DATA[" + std::to_string(i) + "]";
      auto P = _blif.brams[b].bindings.find(port);
      if (P == _blif.brams[b].bindings.end()) {
        fprintf(stderr, "<warning> bram '%s' is disconnected\n", port.c_str());
      } else {
        output2src[P->second] = v2i(2,b);
      }
    }
  }
  // -> inputs
  ForArray(_blif.inputs, i) {
    output2src[_blif.inputs[i]] = v2i(3, i);
  }
  // number all outputs  
  // prepare to create luts
  // -> find register outputs that depend on other registers
  for (const auto& o : output2src) {
    if (o.second[0] == 1) { // latch
      // find input type
      sl_assert(output2src.count(_blif.latches[o.second[1]].input));
      const auto& I = output2src.find(_blif.latches[o.second[1]].input);
      if (I->second[0] == 1) {
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
  vector<int> lut_gates;
  for (const auto& o : output2src) {
    if (o.second[0] == 1) { // latch
      // find input type
      const auto& I = output2src.find (_blif.latches[o.second[1]].input);
      sl_assert(I != output2src.end());
      sl_assert(I->second[0] != 1); // other has to not be a latch
      /// create LUT for the D output (latch input)
      /// assign output to Q (latch output)
      // store indices of input (D) and output (Q)
      _indices[I->first] = (((int)lut_gates.size()) << 1);
      _indices[o.first]  = (((int)lut_gates.size()) << 1) + 1;
      // gate that corresponds to the lut
      lut_gates.push_back(I->second[1]);
    }
  }
  // -> create one LUT per comb output
  for (const auto& o : output2src) {
    if (o.second[0] != 1) { // not latch
      // ignore clock
      if (o.first == "clock") {
        continue;
      }
      // check if the output is already assigned
      if (_indices.count(o.first)) {
        continue;
      }
      // check that it does not use clock // NOTE: investigate
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
      _indices[o.first] = (((int)lut_gates.size()) << 1);
      if (o.second[0] == 0) {        
        lut_gates.push_back(o.second[1]); // gate that corresponds to the lut
      } else if (o.second[0] == 2) {
        lut_gates.push_back(-1); // external lut
      } else if (o.second[0] == 3) {
        lut_gates.push_back(-1); // external lut
      } else {
        fprintf(stderr, "<error> unexpected\n");
      }
    }
  }
  // -> connect BRAMs to design
  for (const auto &b : _blif.brams) {
    _brams.push_back(t_bram());
    _brams.back().name = b.name;
    _brams.back().data = b.data;
    // -> list what to connect
    vector< tuple<string, uint, vector<int>* > > ports_width;
    ports_width.push_back(make_tuple("RD_ADDR", b.addr_width, &_brams.back().rd_addr));
    ports_width.push_back(make_tuple("RD_DATA", b.data_width, &_brams.back().rd_data));
    ports_width.push_back(make_tuple("WR_DATA", b.data_width, &_brams.back().wr_data));
    ports_width.push_back(make_tuple("WR_EN",   b.data_width, &_brams.back().wr_en));
    // -> check and connect
    for (auto &pw : ports_width) {
      for (int i=0; i < (int)get<1>(pw); ++i) {
        string port = get<0>(pw) + "[" + std::to_string(i) + "]";
        auto P = b.bindings.find(port);
        if (P == b.bindings.end()) {
          fprintf(stderr, "<warning> bram '%s' is disconnected\n", port.c_str());
        } else {
          auto I = _indices.find(P->second);
          if (I == _indices.end()) {
            fprintf(stderr, "<error> bram '%s' is connected to unkown '%s'\n", port.c_str(), P->second.c_str());
            exit(-1);
          } else {
            // std::cerr << P->first << " " << I->second << '\n';
            get<2>(pw)->push_back(I->second);
          }
        }
      }
    }
  }
  // -> instantiate LUTs
  for (auto g : lut_gates) {
    _luts.push_back(t_lut());
    ForIndex(i, 4) {
      _luts.back().inputs[i] = -1;
    }
    if (g == -1) {
      _luts.back().cfg = 0;
      _luts.back().external = true; // TODO FIXME: merge with above!!!
    } else {
      _luts.back().cfg = lut_config(_blif.gates[g].config_strings);
      _luts.back().external = false;
      int i = 4 - (int)_blif.gates[g].inputs.size();
      for (auto inp : _blif.gates[g].inputs) {
        auto I = _indices.find(inp);
        if (I == _indices.end()) {
          fprintf(stderr, "<warning> input '%s' disconnected\n", inp.c_str());
        } else {
          _luts.back().inputs[i++] = I->second;
        }
      }
    }
  }

  for (auto op : _blif.outputs) {
    auto I = _indices.find(op);
    if (I == _indices.end()) {
      fprintf(stderr, "<warning> outport '%s' disconnected\n", op.c_str());
    } else {
      _outbits.push_back(make_pair(op, I->second));
    }
  }

  for (const auto& l : _blif.latches) {
    if (l.init == "1") {
      auto I = _indices.find(l.output);
      sl_assert(I != _indices.end());
      _ones.push_back(I->second);
    }
  }

  /// DEBUG
#if 1
  map<int, string> reverse_indices;
  for (auto& idc : _indices) {
    reverse_indices[idc.second] = idc.first;
  }
  for (int l = 0; l < _luts.size(); ++l) {
    fprintf(stderr,"LUT %3d (%s), cfg:%4x, inputs: %4d %4d %4d %4d ext:%d\n",
      l<<1, reverse_indices.at(l<<1).c_str(), _luts[l].cfg,
      _luts[l].inputs[0], _luts[l].inputs[1],
      _luts[l].inputs[2], _luts[l].inputs[3],
      _luts[l].external);
  }
#endif
}

// -----------------------------------------------------------------------------

/*
  Reads the design from a BLIF file
*/
void readDesign(
  vector<t_lut>&              _luts,
  vector<t_bram>&             _brams,
  vector<pair<string, int> >& _outbits,
  vector<int>&                _ones,
  map<string, int>&           _indices)
{
  t_blif blif;
  // parse the blif file
  parse(SRC_PATH "/build/synth.blif", blif);
  // build the design datastructure
  buildSimulData(blif, _luts, _brams, _outbits, _ones, _indices);
}

// -----------------------------------------------------------------------------
