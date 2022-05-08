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

// forward def
void simulPosEdgeAll_cpu(const vector<t_lut>&  luts, vector<uchar>& _outputs);

// -----------------------------------------------------------------------------

static inline void simulLUT_cpu(
  int                   l,
  const vector<t_lut>&  luts,
  vector<uchar>&       _outputs)
{
  // read inputs
  unsigned short cfg_idx = 0;
  for (int i = 0; i < 4; ++i) {
    if (luts[l].inputs[i] > -1) {
      int lut = luts[l].inputs[i] >> 1;
      int q_else_d = luts[l].inputs[i] & 1;
      uchar bit = (_outputs[lut] >> q_else_d) & 1;
      cfg_idx |= bit ? (1 << (3 - i)) : 0;
    }
  }
  // update outputs
  uchar new_value = (luts[l].cfg >> cfg_idx) & 1;
  if (new_value) _outputs[l] |= 1;
  else           _outputs[l] &= 0xfffffffe;
}

// -----------------------------------------------------------------------------

// add LUT fanout to the compute lists
static inline void addFanout(
  int                   l,
  int                   q_else_d,
  const vector<uchar>&  depths,
  int                   numdepths,
  const vector<int>&    fanout,
  vector<int>&         _computelists,
  vector<uchar>&       _outputs
) {
  int cur   = fanout[l];
  int other = fanout[cur];
  while (other != -1) {
    int other_lut = other >> 1;
    if (q_else_d == (other&1)) { // other uses D/Q input
      if ((_outputs[other_lut] & 4) == 0) { // not yet inserted
        _outputs[other_lut] |= 4; // tag as inserted
        // insert in comb. depth compute list
        int dpt = depths[other_lut];
        int cls = _computelists[dpt];
        int idx = _computelists[cls]++;
        _computelists[cls + 1 + idx] = other_lut;
      }
    }
    ++cur;
    other = fanout[cur];
  }
}

// -----------------------------------------------------------------------------

static inline void simulLUT_cpu(
  int                   l,
  const vector<t_lut>&  luts,
  const vector<uchar>&  depths,
  int                   numdepths,
  const vector<int>&    fanout,
  vector<int>&         _computelists,
  vector<uchar>&       _outputs)
{
  // read inputs
  unsigned short cfg_idx = 0;
  for (int i = 0; i < 4; ++i) {
    if (luts[l].inputs[i] > -1) {
      int lut      = luts[l].inputs[i] >> 1;
      int q_else_d = luts[l].inputs[i] & 1;
      uchar bit    = (_outputs[lut] >> q_else_d)&1;
      cfg_idx |= bit ? (1 << (3 - i)) : 0;
    }
  }
  // update outputs
  uchar new_value = (luts[l].cfg >> cfg_idx) & 1;
  if ((_outputs[l]&1) != new_value) {
    if (new_value) _outputs[l] |=  1;
    else           _outputs[l] &= ~1;
    // add fanout to compute list
    addFanout(l, 0, depths, numdepths, fanout, _computelists, _outputs);
    // add this LUT to posedge list
    if ((_outputs[l] & 8) == 0) { // not yet inserted
      _outputs[l] |= 8; // tag as inserted
      // insert in posedge compute list
      int dpt = numdepths;
      int cls = _computelists[dpt];
      int idx = _computelists[cls]++;
      _computelists[cls + 1 + idx] = l;
    }
  }
  // reset inserted flag (preserve posedge flag)
  _outputs[l] &= 3|8;
}

// -----------------------------------------------------------------------------

void simulInit_cpu(
  const vector<t_lut>& luts,
  const vector<int>&   step_starts,
  const vector<int>&   step_ends,
  const vector<int>&   ones,
  vector<int>&        _computelists,
  vector<uchar>&      _outputs)
{
  _outputs.resize(luts.size(),0);
  // initialize ones
  for (int o = 0; o < ones.size(); ++o) {
    _outputs[ones[o] >> 1] |= 1 << (ones[o] & 1);
  }
  // resolve const cells
  for (int cy = 0; cy < 2; ++cy) { // those which are const, and then those that only depend on consts
    for (int l = step_starts[0]; l <= step_ends[0]; ++l) {
      simulLUT_cpu(l, luts, _outputs);
    }
    simulPosEdgeAll_cpu(luts, _outputs);
  }
  // initialize ones
  // Why a second time? Some of these registers may have been cleared after const resolve
  for (int o = 0; o < ones.size(); ++o) {
    _outputs[ones[o] >> 1] |= 1 << (ones[o] & 1);
  }
  // computelists
  int cpl_sz = (int)step_starts.size()+1; // header, one index per depth + 1 for posedge
  for (int d = 0; d < step_starts.size(); ++d) {
    int num = step_ends[d] - step_starts[d] + 1; // max entries in list for this depth
    cpl_sz += num + 1; // +1 for list length
  }
  cpl_sz += (int)luts.size() + 1; // final list for posedge
  _computelists.resize(cpl_sz,0);
  int offset = (int)step_starts.size()+1; // header, one index per depth + 1 for posedge
  for (int d = 0; d < step_starts.size(); ++d) {
    _computelists[d] = offset; // header, start of list (first entry is length)
    int num = step_ends[d] - step_starts[d] + 1; // max entries in list for this depth
    offset += num + 1; // +1 for list length
  }
  _computelists[step_starts.size()] = offset; // final list for posedge
  // -> initially we put all LUTs in the computelist
  for (int d = 0; d < (int)step_starts.size() ; ++d) {
    int cls = _computelists[d];
    // fill-in list
    for (int l = step_starts[d]; l <= step_ends[d]; ++l) {
      int idx = _computelists[cls]++;
      sl_assert(cls + 1 + idx < _computelists.size());
      _computelists[cls + 1 + idx] = l;
    }
  }
  {
    int cls = _computelists[step_starts.size()];
    for (int l = 0; l < luts.size(); ++l) {
      int idx = _computelists[cls]++;
      sl_assert(cls + 1 + idx < _computelists.size());
      _computelists[cls + 1 + idx] = l;
    }
  }
  // -> we tag all LUTs as being inserted already
  for (int l = 0; l < luts.size(); ++l) {
    _outputs[l] |= 4 | 8;
  }
}

// -----------------------------------------------------------------------------

void simulCycle_cpu(
  const vector<t_lut>& luts,
  const vector<uchar>& depths,
	const vector<int>&   step_starts,
	const vector<int>&   step_ends,
  const vector<int>&   fanout,
  vector<int>&        _computelists,
	vector<uchar>&      _outputs)
{
  for (int depth = 0; depth < step_starts.size(); ++depth) {
    // process LUTs
    int cls = _computelists[depth];
    int num = _computelists[cls];
    // cerr << sprint("depth: %5d, num: %5d\n", depth, num);
    for (int n = 0; n < num ; ++n) {
      int l = _computelists[cls + 1 + n];
      simulLUT_cpu(l, luts, depths, (int)step_starts.size(), fanout, _computelists, _outputs);
    }
    // clear compute list for this depth
    _computelists[cls] = 0;
  }
}

// -----------------------------------------------------------------------------

void simulPosEdgeAll_cpu(
  const vector<t_lut>&  luts,
  vector<uchar>&       _outputs)
{
  for (int l = 0; l < luts.size(); ++l) {
    uchar d = _outputs[l] & 1;
    uchar q = (_outputs[l] >> 1) & 1;
    if (d != q) {
      if (d) {
        _outputs[l] |= 2;
      } else {
        _outputs[l] &= 0xfffffffd;
      }
    }
  }
}

// -----------------------------------------------------------------------------

void simulPosEdge_cpu(
  const vector<t_lut>&  luts,
  const vector<uchar>&  depths,
  int                   numdepths,
  const vector<int>&    fanout,
  vector<int>&         _computelists,
  vector<uchar>&       _outputs)
{
  int cls = _computelists[numdepths];
  // process LUTs
  int num = _computelists[cls];
  // cerr << sprint("posedge    num: %5d\n", num);
  for (int n = 0; n < num; ++n) {
    int l = _computelists[cls + 1 + n];
    uchar d = _outputs[l] & 1;
    uchar q = (_outputs[l] >> 1) & 1;
    if (d != q) {
      if (d) {
        _outputs[l] |= 2;
      } else {
        _outputs[l] &= 0xfffffffd;
      }
      // add fanout to compute list
      addFanout(l, 1, depths, numdepths, fanout, _computelists, _outputs);
    }
    // reset inserted flag
    _outputs[l] &= 7;
  }
  // clear compute list
  _computelists[cls] = 0;
}

// -----------------------------------------------------------------------------

void simulPrintOutput_cpu(
  const vector<uchar>&             outputs,
  const vector<pair<string,int> >& outbits)
{
  // display result
  int val = 0;
  string str;
  for (int b = 0; b < outbits.size() ; b++) {
    int lut      = outbits[b].second >> 1;
    int q_else_d = outbits[b].second & 1;
    uchar bit = (outputs[lut] >> q_else_d) & 1;
    str = (bit ? "1" : "0") + str;
    val += bit << b;
  }
  fprintf(stderr,"b%s (d%03d h%03x)   \n",str.c_str(),val,val);
}

// -----------------------------------------------------------------------------
