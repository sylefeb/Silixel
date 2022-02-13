// @sylefeb 2022-01-04

#pragma once

#include <vector>
#include <string>

typedef unsigned char uchar;

#pragma pack(push)
#pragma pack(1)
typedef struct s_lut {
  unsigned short cfg;
  int            inputs[4];
} t_lut;
#pragma pack(pop)

void readDesign(
  std::vector<t_lut>&                       _luts,
  std::vector<std::pair<std::string,int> >& _outbits,
  std::vector<int>&                         _ones);
