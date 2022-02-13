// @sylefeb 2022-01-04

#pragma once

#include <vector>
#include <string>
using namespace std;

#include "read.h"
#include "analyze.h"

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
