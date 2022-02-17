// @sylefeb 2022-01-04
/*

Analyzes the design, determines the 'depth' of each LUT by propagating
from the Q outputs (depth 0). The LUT depth is 1 + the max of its input depths.
Within a clock cycle:
- LUTs of lower depth are not influenced by LUTs of higher depth.
- LUTs at a same depth are not influenced by each others.
The LUTs are then sorted by depth and the data structure reordered.

*/
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

#include <LibSL/LibSL.h>

// -----------------------------------------------------------------------------

// Propagates depths through the network. Returns whether something changed.
bool analyzeStep(const vector<t_lut>& luts,int& _swap,vector<int>& _depths)
{
  bool changed = false;
  int r_off = _swap ? ((int)luts.size()<<1) : 0; // read from offset
  int w_off = _swap ? 0 : ((int)luts.size()<<1); // write to offset
  for (int l=0;l<luts.size();++l) {
    // copy output depths
    _depths[w_off + (l<<1) + 0] = _depths[r_off + (l<<1) + 0];
    _depths[w_off + (l<<1) + 1] = _depths[r_off + (l<<1) + 1];
    // read input depths
    unsigned short cfg_idx = 0;
    int new_value = 0;
    for (int i=0;i<4;++i) {
      if (luts[l].inputs[i] > -1) {
        int other_value = _depths[r_off + luts[l].inputs[i]];
        if (other_value < std::numeric_limits<int>::max()) ++other_value;
        new_value       = max(new_value , other_value);
      }
    }
    // update D outputs
    if (_depths[r_off + (l<<1) + 0] != new_value) {
      _depths[w_off + (l<<1) + 0] = new_value;
      changed = true;
    }
    // flip-flops Q forced to depth zero
    _depths[w_off + (l<<1) + 1] = 0;
  }
  _swap = 1 - _swap;
  return changed;
}

// -----------------------------------------------------------------------------

void analyze(const vector<t_lut>& luts,
  const vector<int>& ones,
  vector<int>&      _reorder,
  vector<int>&      _inv_reorder,
  vector<int>&      _step_starts,
  vector<int>&      _step_ends,
  vector<uchar>&    _depths)
{
  int swap = 0;
  vector<int> outputs;
  outputs.resize(luts.size() << 2, // x2 (D,Q), x2 buffers
                 std::numeric_limits<int>::max());
  // iterate
  bool changed = true;
  int maxiter = 256;
  while (changed && maxiter-- > 0) {
    changed = analyzeStep(luts, swap, outputs);
  }
  if (maxiter <= 0) {
    fprintf(stderr, "cannot perform analysis, loop?");
    exit(-1);
  }
  // reorder by increasing depth
  vector<pair<int, int> > source;
  source.resize(luts.size());
  int max_depth = 0;
  for (int l = 0; l < luts.size(); ++l) {
    source[l] = make_pair(outputs[(l << 1) + 0], l); // depth,id
    if (outputs[(l << 1) + 0] < std::numeric_limits<int>::max()) {
      max_depth = max(max_depth, outputs[(l << 1) + 0]);
    }
  }
  if (max_depth == 0) {
    fprintf(stderr, "cannot perform analysis");
    exit(-1);
  }
  /// asjust based on initialization
  // we can only consider const if the inputs where not initialized, otherwise
  // there may be a one-purpose cascade of FF from the initialization point
  set<int> with_init;
  for (auto one : ones) {
    with_init.insert(one);
  }
  // promote 0-depth cells with init to 1-depth
  for (int l = 0; l < luts.size(); ++l) {
    if (source[l].first == 0) {
      if (with_init.count((l << 1) + 0) || with_init.count((l << 1) + 1)) {
        source[l].first = 1;
        break;
      }
    }
  }
  // convert d-depth cells using only 0-depth const cells as 0-depth
  for (int depth = 1; depth <= max_depth; depth++) {
    for (int l = 0; l < luts.size(); ++l) {
      if (source[l].first == depth) {
        bool no_init_input = true;
        for (int i = 0; i < 4; ++i) {
          if (luts[l].inputs[i] > -1) {
            if (with_init.count(luts[l].inputs[i]) != 0) {
              no_init_input = false; break;
            }
          }
        }
        if (no_init_input) {
          // now we check that all inputs are 0-depth
          bool all_inputs_0depth = true;
          for (int i = 0; i < 4; ++i) {
            if (luts[l].inputs[i] > -1) {
              int idepth = source[luts[l].inputs[i] >> 1].first;
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

  sort(source.begin(),source.end());

  _reorder    .resize(luts.size());
  _inv_reorder.resize(luts.size());
  _depths     .resize(luts.size());
  _step_starts.resize(max_depth+1,std::numeric_limits<int>::max());
	_step_ends  .resize(max_depth+1,0);
  for (int o=0;o<_reorder.size();++o) {
    _reorder[o]                    = source[o].second;
    _inv_reorder[source[o].second] = o;
    _depths[o]                     = source[o].first;
    if (source[o].first < std::numeric_limits<int>::max()) {
      _step_starts[source[o].first] = min(_step_starts[source[o].first],o);
      _step_ends  [source[o].first] = max(_step_ends  [source[o].first],o);
    }
  }

  // debug
  fprintf(stderr,"analysis done\n");

  for (int d=0;d<_step_starts.size();++d) {
    fprintf(stderr,"depth %3d on luts %6d-%6d (%6d/%6d)\n",
      d,_step_starts[d],_step_ends[d],
      _step_ends[d] - _step_starts[d] + 1,
      (int)luts.size());
  }
}

// -----------------------------------------------------------------------------

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
