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
