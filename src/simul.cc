// @sylefeb 2022-01-04

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

#include "simul.h"
#include "blif.h"

// -----------------------------------------------------------------------------

void simulPosEdgeAll_cpu(
  const vector<t_lut>&  luts,
  vector<uchar>&       _outputs);

// -----------------------------------------------------------------------------

void buildSimulData(
  t_blif&                     _blif, // might change
  vector<t_lut>&              _luts,
  vector<pair<string, int> >& _outbits,
  vector<int>&                _ones)
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
      auto& I = output2src.find(_blif.latches[o.second[1]].input);
      // cerr << o.first << " <:: " << I->first << '\n';
      if (I->second[0]) {
        // input of this latch is the output (Q) of an earlier latch
        // we need a passtrhrough gate to do that
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
      auto& I = output2src.find (_blif.latches[o.second[1]].input);
      // cerr << o.first << " <:: " << I->first << '\n';
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
      // check that it does not use clock (really??)
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

void readDesign(vector<t_lut>& _luts, vector<pair<string,int> >& _outbits, vector<int>& _ones)
{
  t_blif blif;
  parse(SRC_PATH "/build/synth.blif",blif);

  buildSimulData(blif, _luts, _outbits, _ones);

}

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

static inline void addFanout(
  int                   l,
  int                   q_else_d,
  const vector<uchar>&  depths,
  int                   numdepths,
  const vector<int>&    fanout,
  vector<int>&         _computelists,
  vector<uchar>&       _outputs
) {
  // add fanout to compute list
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
    {
      // add to posedge list
      if ((_outputs[l] & 8) == 0) { // not yet inserted
        _outputs[l] |= 8; // tag as inserted
        // insert in posedge compute list
        int dpt = numdepths;
        int cls = _computelists[dpt];
        int idx = _computelists[cls]++;
        _computelists[cls + 1 + idx] = l;
      }
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

bool analyzeStep(const vector<t_lut>& luts,int& _swap,vector<int>& _outputs)
{
  bool changed = false;
  int r_off = _swap ? ((int)luts.size()<<1) : 0;
  int w_off = _swap ? 0 : ((int)luts.size()<<1);
  for (int l=0;l<luts.size();++l) {
    // copy outputs
    _outputs[w_off + (l<<1) + 0] = _outputs[r_off + (l<<1) + 0];
    _outputs[w_off + (l<<1) + 1] = _outputs[r_off + (l<<1) + 1];
    // read inputs
    unsigned short cfg_idx = 0;
    int new_value = 0;
    for (int i=0;i<4;++i) {
      if (luts[l].inputs[i] > -1) {
        int other_value = _outputs[r_off + luts[l].inputs[i]];
        if (other_value < std::numeric_limits<int>::max()) ++other_value;
        new_value       = max(new_value , other_value);
      }
    }
    // update comb outputs
    if (_outputs[r_off + (l<<1) + 0] != new_value) {
      _outputs[w_off + (l<<1) + 0] = new_value;
      changed = true;
    }
    // flip-flops Q forced to zero
    _outputs[w_off + (l<<1) + 1] = 0;
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
  outputs.resize(luts.size() << 2, std::numeric_limits<int>::max()); // x2 (out,reg), x2 buffers
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
