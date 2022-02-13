// @sylefeb 2022-01-04

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
