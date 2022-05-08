// @sylefeb 2022-01-04
/* ---------------------------------------------------------------------

Analyzes the design, determines the 'depth' of each LUT by propagating
from the Q outputs (depth 0). The LUT depth is 1 + the max of its input depths.
Within a clock cycle:
- LUTs of lower depth are not influenced by LUTs of higher depth.
- LUTs at a same depth are not influenced by each others.
The LUTs are then sorted by depth and the data structure reordered.

 ----------------------------------------------------------------------- */
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
#include "analyze.h"

// -----------------------------------------------------------------------------

// Propagates depths through the network (from all Q at depth 0).
// Returns whether something changed.
bool analyzeStep(const vector<t_lut>& luts,vector<int>& _depths)
{
  bool changed = false;
  for (int l=0;l<luts.size();++l) {
    // read input depths
    unsigned short cfg_idx = 0;
    int new_value = 0;
    for (int i=0;i<4;++i) {
      if (luts[l].inputs[i] > -1) {
        int other_value = 0;
        if ((luts[l].inputs[i] & 1) == 0) {
          other_value = _depths[(luts[l].inputs[i] >> 1)];
        }
        if (other_value < std::numeric_limits<int>::max()) {
          ++other_value;
        }
        new_value = max(new_value , other_value);
      }
    }
    // update output depth if changed
    if (_depths[l] != new_value) {
      _depths[l] = new_value;
      changed = true;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------

// Performs an analysis of the design, computing the combinational depth
// of all LUTs
void analyze(
  vector<t_lut>&    _luts,
  std::vector<pair<std::string, int> >& _outbits,
  vector<int>&      _ones,
  vector<int>&      _step_starts,
  vector<int>&      _step_ends,
  vector<uchar>&    _depths)
{
  vector<int> lut_depths;
  lut_depths.resize(_luts.size(),std::numeric_limits<int>::max());
  /// iterate the analysis step
  // propagates combinational depth from Q outputs
  bool changed = true;
  int maxiter = 1024;
  while (changed && maxiter-- > 0) {
    changed = analyzeStep(_luts, lut_depths);
  }
  if (maxiter <= 0) {
    fprintf(stderr, "cannot perform analysis, combinational loop in design?");
    exit(-1);
  }
  // reorder by increasing depth
  vector<pair<int, int> > source;
  source.resize(_luts.size());
  int max_depth = 0;
  for (int l = 0; l < _luts.size(); ++l) {
    source[l] = make_pair(lut_depths[l], l); // depth,id
    if (lut_depths[l] < std::numeric_limits<int>::max()) {
      max_depth = max(max_depth, lut_depths[l]);
    }
  }
  if (max_depth == 0) {
    fprintf(stderr, "analysis failed (why?)");
    exit(-1);
  }
  /// determine const LUTs based on initialization
  // const LUTs are placed at depth 0, which is not simulated
  // we can only consider const if the inputs where not initialized, otherwise
  // there may be an on-purpose cascade of FF from the initialization point
  set<int> with_init;
  for (auto one : _ones) {
    with_init.insert(one);
  }
  // promote 0-depth cells with init to 1-depth (non const)
  for (int l = 0; l < _luts.size(); ++l) {
    if (source[l].first == 0) {
      if (with_init.count((l << 1) + 0) || with_init.count((l << 1) + 1)) {
        source[l].first = 1;
        break;
      }
    }
  }
  // convert d-depth cells using only 0-depth const cells as 0-depth (const)
  for (int depth = 1; depth <= max_depth; depth++) {
    for (int l = 0; l < _luts.size(); ++l) {
      if (source[l].first == depth) {
        bool no_init_input = true;
        for (int i = 0; i < 4; ++i) {
          if (_luts[l].inputs[i] > -1) {
            if (with_init.count(_luts[l].inputs[i]) != 0) {
              no_init_input = false; break;
            }
          }
        }
        if (no_init_input) {
          // now we check that all inputs are 0-depth
          bool all_inputs_0depth = true;
          for (int i = 0; i < 4; ++i) {
            if (_luts[l].inputs[i] > -1) {
              int idepth = source[_luts[l].inputs[i] >> 1].first;
              if (idepth > 0) {
                all_inputs_0depth = false;
              }
            }
          }
          if (all_inputs_0depth) {
            source[l].first = 0;
          }
        }
      }
    }
  }

#if 0
  // debug: output full list of LUTs
  for (int l = 0; l < luts.size(); ++l) {
    int i0d = luts[l].inputs[0] < 0 ? 999 : (luts[l].inputs[0] & 1 ? 999 : source[luts[l].inputs[0] >> 1].first);
    int i1d = luts[l].inputs[1] < 0 ? 999 : (luts[l].inputs[1] & 1 ? 999 : source[luts[l].inputs[1] >> 1].first);
    int i2d = luts[l].inputs[2] < 0 ? 999 : (luts[l].inputs[2] & 1 ? 999 : source[luts[l].inputs[2] >> 1].first);
    int i3d = luts[l].inputs[3] < 0 ? 999 : (luts[l].inputs[3] & 1 ? 999 : source[luts[l].inputs[3] >> 1].first);
    fprintf(stderr, "LUT %d, depth %d min input depths: %d\n",
      l, source[l].first, min(min(i0d, i1d), min(i2d, i3d)));
  }
#endif
  // sort by depth
  sort(source.begin(),source.end());
  // build the reordering arrays
  vector<int> reorder;
  vector<int> inv_reorder;
  reorder     .resize(_luts.size());
  inv_reorder .resize(_luts.size());
  _depths     .resize(_luts.size());
  _step_starts.resize(max_depth+1,std::numeric_limits<int>::max());
	_step_ends  .resize(max_depth+1,0);
  for (int o=0;o<reorder.size();++o) {
    reorder[o]                    = source[o].second;
    inv_reorder[source[o].second] = o;
    _depths[o]                    = source[o].first;
    if (source[o].first < std::numeric_limits<int>::max()) {
      _step_starts[source[o].first] = min(_step_starts[source[o].first],o);
      _step_ends  [source[o].first] = max(_step_ends  [source[o].first],o);
    }
  }
  // reorder the LUTs
  vector<t_lut> init_luts = _luts;
  reorderLUTs(init_luts, reorder, inv_reorder, _luts, _outbits, _ones);
  // print report
  fprintf(stderr,"analysis done\n");
  for (int d=0;d<_step_starts.size();++d) {
    fprintf(stderr,"depth %3d on luts %6d-%6d (%6d/%6d)\n",
      d,_step_starts[d],_step_ends[d],
      _step_ends[d] - _step_starts[d] + 1,
      (int)_luts.size());
  }
}

// -----------------------------------------------------------------------------

// Reorders the LUT datastructure based on input reordering arrays
void reorderLUTs(
  const vector<t_lut>&       init_luts,
  const vector<int>&         reorder,
  const vector<int>&         inv_reorder,
  vector<t_lut>&             _luts,
  vector<pair<string,int> >& _outbits,
  vector<int>&               _ones)
{
  /// apply the reordering
  // -> luts
  _luts.resize(init_luts.size());
  for (int o=0;o<reorder.size();++o) {
    int l        = reorder[o];
    _luts[o].cfg = init_luts[l].cfg;
    for (int i = 0; i < 4 ; ++i) {
      if (init_luts[l].inputs[i] > -1) {
        int reg = init_luts[l].inputs[i] &1;
        int src = init_luts[l].inputs[i]>>1;
        _luts[o].inputs[i] = (inv_reorder[src]<<1) | reg;
      } else {
        _luts[o].inputs[i] = -1;
      }
    }
  }
  // -> bits
  for (int b = 0; b < _outbits.size(); ++b) {
    int reg            = _outbits[b].second &1;
    int src            = _outbits[b].second>>1;
    _outbits[b].second = (inv_reorder[src]<<1) | reg;
  }
  // -> ones (init)
  for (int b = 0; b < _ones.size(); ++b) {
    int reg  = _ones[b] & 1;
    int src  = _ones[b] >> 1;
    _ones[b] = (inv_reorder[src] << 1) | reg;
  }
}

// -----------------------------------------------------------------------------


// Builds a data-structure representing the fanout of each LUT: the list
// of LUTs that use it as an input. This is used to only simulate the LUTs
// which inputs have changed at each depth.
void buildFanout(
  vector<t_lut>&             _luts,
  vector<int>&               _fanout)
{
  // build fanout
  vector<set<int> > fanouts;
  fanouts.resize(_luts.size());
  for (int l = 0; l < _luts.size(); ++l) {
    ForIndex(i, 4) {
      if (_luts[l].inputs[i] > -1) {
        int lut_in = _luts[l].inputs[i] >> 1;
        int lut_in_q_else_d = _luts[l].inputs[i] & 1;
        fanouts[lut_in].insert((l << 1) | lut_in_q_else_d);
      }
    }
  }
  // -> flatten in output
  int totsz = 0;
  for (const auto& fo : fanouts) {
    totsz += (int)fo.size() + 1;
  }
  _fanout.reserve(_luts.size() /*header, 1 index per lut*/ + totsz);
  int rsum = (int)_luts.size();
  for (const auto& fo : fanouts) {
    _fanout.push_back(rsum);
    rsum += (int)fo.size() + 1;
  }
  for (const auto& fo : fanouts) {
    for (auto l : fo) {
      _fanout.push_back(l);
    }
    _fanout.push_back(-1);
  }
  sl_assert(_fanout.size() == _luts.size() + totsz);
}

// -----------------------------------------------------------------------------
