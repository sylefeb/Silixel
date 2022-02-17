// @sylefeb 2022-01-04

#pragma once

#include <vector>
#include <string>

typedef unsigned char uchar;

#pragma pack(push)
#pragma pack(1)
// struct holding a LUT configuration:
// - cfg is a 16 bits integer that defined the truth table for 4 inputs
// - inputs[4] are the indices of the inputs (other LUTs in the LUT table)
//   Each index lower bit indicates whether the input is connected to D (0)
//   or Q (1). The higher bits are the LUT index.
//   So for LUT i the index is obtained as (i<<1) + 0 if connected to D
//                                      or (i<<1) + 1 if connected to Q
//   Given the index x, the LUT is (x>>1) and (x&1) == 1 if Q, otherwise D
typedef struct s_lut {
  unsigned short cfg;
  int            inputs[4];
} t_lut;
#pragma pack(pop)

void readDesign(
  std::vector<t_lut>&                       _luts,
  std::vector<std::pair<std::string,int> >& _outbits,
  std::vector<int>&                         _ones);
