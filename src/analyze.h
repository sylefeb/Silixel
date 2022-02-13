// @sylefeb 2022-01-04

#pragma once

#include <vector>
using namespace std;

void analyze(
  const std::vector<t_lut>& init_luts,
  const std::vector<int>& ones,
  std::vector<int>&   _reorder,
  std::vector<int>&   _inv_reorder,
  std::vector<int>&   _step_starts,
  std::vector<int>&   _step_ends,
  std::vector<uchar>& _depths);

void reorderLUTs(
  const std::vector<t_lut>&        init_luts,
  const std::vector<int>&          reorder,
  const std::vector<int>&          inv_reorder,
  std::vector<t_lut>&             _luts,
  std::vector<pair<std::string,int> >& _outbits,
  std::vector<int>&               _ones);

void buildFanout(
  std::vector<t_lut>&             _luts,
  std::vector<int>&               _fanout);
