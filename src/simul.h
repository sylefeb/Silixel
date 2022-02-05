// @sylefeb 2022-01-04

#pragma once

#include <vector>
using namespace std;

typedef unsigned char uchar;

#pragma pack(push)
#pragma pack(1)
typedef struct s_lut {
  unsigned short cfg;
  int            inputs[4];
} t_lut;
#pragma pack(pop)

void readDesign(
  vector<t_lut>&             _luts, 
  vector<pair<string,int> >& _outbits, 
  vector<int>&               _ones);

void simulInit_cpu(
  const vector<t_lut>& luts,
  const vector<int>&  step_starts,
  const vector<int>&  step_ends,
  const vector<int>&  ones,
  vector<int>&       _computelists,
  vector<uchar>&     _outputs);

void simulCycle_cpu(
  const vector<t_lut>& luts,
  const vector<uchar>& depths,
  const vector<int>&  step_starts,
  const vector<int>&  step_ends,
  const vector<int>&  fanout,
  vector<int>&       _computelists,
  vector<uchar>&     _outputs);

void simulPosEdge_cpu(
  const vector<t_lut>&  luts,
  const vector<uchar>&  depths,
  int                   numdepths,
  const vector<int>&    fanout,
  vector<int>&         _computelists,
  vector<uchar>&       _outputs);

void simulPrintOutput_cpu(
  const vector<uchar>& outputs, 
  const vector<pair<string,int> >& outbits);

void analyze(
  const vector<t_lut>& init_luts,
  const vector<int>& ones,
  vector<int>&   _reorder,
  vector<int>&   _inv_reorder,
  vector<int>&   _step_starts,
  vector<int>&   _step_ends,
  vector<uchar>& _depths);

void reorderLUTs(
  const vector<t_lut>&        init_luts,
  const vector<int>&          reorder,
  const vector<int>&          inv_reorder,
  vector<t_lut>&             _luts,
  vector<pair<string,int> >& _outbits,
  vector<int>&               _ones);

void buildFanout(
  vector<t_lut>&             _luts,
  vector<int>&               _fanout);
