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
