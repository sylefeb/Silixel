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

// --------------------------------------------------------------

#include <vector>
using namespace std;

// --------------------------------------------------------------

#define CYCLE_BUFFER_LEN 1024

// --------------------------------------------------------------

#include "sh_simul.h"
extern AutoBindShader::sh_simul      g_ShSimul;
#include "sh_posedge.h"
extern AutoBindShader::sh_posedge    g_ShPosEdge;
#include "sh_outports.h"
extern AutoBindShader::sh_outports   g_ShOutPorts;
#include "sh_init.h"
extern AutoBindShader::sh_init       g_ShInit;

typedef GPUMESH_MVF2(mvf_vertex_2f, mvf_texcoord0_2f) mvf_simple;
typedef GPUMesh_GL_VBO<mvf_simple>  GLMesh;

extern AutoPtr<GLMesh>               g_Quad;

extern GLTimer                       g_GPU_timer;

// --------------------------------------------------------------

void simulInit_gpu(
  const vector<t_lut>& luts,
  const vector<int>&   ones
);

void simulBegin_gpu(
  const vector<t_lut>& luts,
  const vector<int>&   step_starts,
  const vector<int>&   step_ends,
  const vector<int>&   ones);

void simulCycle_gpu(
  const vector<t_lut>& luts,
  const vector<int>&   step_starts,
  const vector<int>&   step_ends);

bool simulReadback_gpu();

void simulPrintOutput_gpu(const vector<pair<string, int> >& outbits);

void simulEnd_gpu();

void simulTerminate_gpu();

// --------------------------------------------------------------
