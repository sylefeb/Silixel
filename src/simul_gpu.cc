// @sylefeb 2022-02-11
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
// --------------------------------------------------------------

#include <LibSL/LibSL.h>
#include <LibSL/LibSL_gl4core.h>

#include "read.h"
#include "analyze.h"
#include "simul_gpu.h"

using namespace std;

// --------------------------------------------------------------

// Ah, some good old globals and externs
// defined in silixel.cc
extern map<string, v2i> g_OutPorts;
extern Array<int>       g_OutPortsValues;
extern int              g_Cycle;

// --------------------------------------------------------------

// Oh, some more globals, sure!
// All shaders
AutoBindShader::sh_simul      g_ShSimul;   // simulates a sub-cycle step
AutoBindShader::sh_posedge    g_ShPosEdge; // simulates posedge
AutoBindShader::sh_outports   g_ShOutPorts;// fills output ports
AutoBindShader::sh_init       g_ShInit;    // init LUTs with ones

// --------------------------------------------------------------

using namespace std;

// --------------------------------------------------------------

// More?? Of course, why not?
GLBuffer g_LUTs_Cfg;         // uint,  one  per LUT (NOTE: 16 bits are used, could pack)
GLBuffer g_LUTs_Addrs;       // uint,  four per LUT (NOTE: 24 bits per addr is enough, could pack on three)
GLBuffer g_LUTs_Outputs;     // uint,  one per LUT, bit 0 (D) bit 1 (Q) bit 2 (dirty)
GLBuffer g_GPU_OutPortsLocs; // uint,  per outport
GLBuffer g_GPU_OutPortsVals; // uint,  per outport * CYCLE_BUFFER_LEN (NOTE: total overkill, could be one bit)
GLBuffer g_GPU_OutInits;     // uint, one per output to initialize

// --------------------------------------------------------------
const int G = 128;
// --------------------------------------------------------------

void simulInit_gpu(const vector<t_lut>& luts,const vector<int>& ones)
{
  g_ShSimul.init();
  g_ShPosEdge.init();
  g_ShOutPorts.init();
  g_ShInit.init();

  int n_luts = (int)luts.size();
  n_luts += ( (n_luts & (G - 1)) ? (G - (n_luts & (G - 1))) : 0 );
  g_LUTs_Cfg    .init( n_luts       * sizeof(uint), GL_SHADER_STORAGE_BUFFER);
  g_LUTs_Addrs  .init((n_luts << 2) * sizeof(uint), GL_SHADER_STORAGE_BUFFER);
  g_LUTs_Outputs.init( n_luts       * sizeof(uint), GL_SHADER_STORAGE_BUFFER);
  g_GPU_OutPortsVals.init((int)g_OutPorts.size() * sizeof(uint) * CYCLE_BUFFER_LEN, GL_SHADER_STORAGE_BUFFER);
  g_GPU_OutPortsLocs.init((int)g_OutPorts.size() * sizeof(uint), GL_SHADER_STORAGE_BUFFER);
  g_GPU_OutInits.init((int)ones.size() * sizeof(uint), GL_SHADER_STORAGE_BUFFER);

  // we initialize all outputs to zero
  {
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, g_LUTs_Outputs.glId());
    int *ptr = (int*)glMapBufferARB(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    memset(ptr, 0x00, g_LUTs_Outputs.size());
    glUnmapBufferARB(GL_SHADER_STORAGE_BUFFER);
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, 0);
  }
  // initialize the static LUT table
  // -> configs
  {
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, g_LUTs_Cfg.glId());
    int *ptr = (int*)glMapBufferARB(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    ForIndex(l, (int)luts.size()) {
      ptr[l] = (int)luts[l].cfg;
    }
    glUnmapBufferARB(GL_SHADER_STORAGE_BUFFER);
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, 0);
  }
  // -> addrs
  {
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, g_LUTs_Addrs.glId());
    int *ptr = (int*)glMapBufferARB(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    ForIndex(l, (int)luts.size()) {
      ForIndex(i, 4) {
        ptr[(l<<2)+i] = max(0,(int)luts[l].inputs[i]);
      }
    }
    glUnmapBufferARB(GL_SHADER_STORAGE_BUFFER);
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, 0);
  }
  // -> outport locations
  {
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, g_GPU_OutPortsLocs.glId());
    int *ptr = (int*)glMapBufferARB(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    for (auto op : g_OutPorts) {
      ptr[op.second[0]] = op.second[1];
    }
    glUnmapBufferARB(GL_SHADER_STORAGE_BUFFER);
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, 0);
  }
  // -> initialized outputs
  {
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, g_GPU_OutInits.glId());
    int *ptr = (int*)glMapBufferARB(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    ForIndex(o,ones.size()) { ptr[o] = ones[o]; }
    glUnmapBufferARB(GL_SHADER_STORAGE_BUFFER);
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, 0);
  }
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

/* -------------------------------------------------------- */

void simulBegin_gpu(
  const vector<t_lut>& luts,
  const vector<int>&   step_starts,
  const vector<int>&   step_ends,
  const vector<int>&   ones)
{
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_LUTs_Cfg.glId());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_LUTs_Addrs.glId());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_LUTs_Outputs.glId());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, g_GPU_OutPortsLocs.glId());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, g_GPU_OutPortsVals.glId());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, g_GPU_OutInits.glId());
  // init cells
  g_ShInit.begin();
  g_ShInit.run(v3i((int)ones.size(),1,1));
  g_ShInit.end();
  // resolve constant cells
  ForIndex (c,2) {
    int n = step_ends[0] - step_starts[0] + 1;
    g_ShSimul.begin();
    g_ShSimul.start_lut.set((uint)0);
    g_ShSimul.num.set((uint)n);
    g_ShSimul.run(v3i((n / G) + ((n & (G - 1)) ? 1 : 0), 1, 1));
    g_ShSimul.end();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    g_ShPosEdge.begin();
    n = (int)luts.size();
    g_ShPosEdge.num.set((uint)n);
    g_ShPosEdge.run(v3i((n / G) + ((n & (G - 1)) ? 1 : 0), 1, 1));
    g_ShPosEdge.end();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
  // init cells
  // Why a second time? Some of these registers may have been cleared after const resolve
  g_ShInit.begin();
  g_ShInit.run(v3i((int)ones.size(), 1, 1));
  g_ShInit.end();
}

/* -------------------------------------------------------- */

/*
Simulate one cycle on the GPU
*/
void simulCycle_gpu(
  const vector<t_lut>& luts,
  const vector<int>&   step_starts,
  const vector<int>&   step_ends)
{

  g_ShSimul.begin();
  // iterate on depth levels (skipping const depth 0)
  ForRange(depth, 1, (int)step_starts.size()-1) {
    // only update LUTs at this particular level
    int n = step_ends[depth] - step_starts[depth] + 1;
    g_ShSimul.start_lut.set((uint)step_starts[depth]);
    g_ShSimul.num.set((uint)n);
    g_ShSimul.run(v3i((n / G) + ((n & (G - 1)) ? 1 : 0), 1, 1));
    // sync required between iterations
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
  g_ShSimul.end();
  // simulate positive clock edge
  g_ShPosEdge.begin();
  int n = (int)luts.size();
  g_ShPosEdge.num.set((uint)n);
  g_ShPosEdge.run(v3i((n / G) + ((n & (G - 1)) ? 1 : 0), 1, 1));
  g_ShPosEdge.end();
  // sync required to ensure further reads see the update
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  ++g_Cycle;

}

/* -------------------------------------------------------- */

void simulEnd_gpu()
{
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
}

/* -------------------------------------------------------- */

uint g_RBCycle = 0;

bool simulReadback_gpu()
{
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // gather outport values
  g_ShOutPorts.begin();
  g_ShOutPorts.offset.set((uint)g_OutPorts.size() * g_RBCycle);
  g_ShOutPorts.run(v3i((int)g_OutPorts.size(), 1, 1)); // TODO: local size >= 32
  g_ShOutPorts.end();

  ++g_RBCycle;

  if (g_RBCycle == CYCLE_BUFFER_LEN) {
    // readback buffer
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, g_GPU_OutPortsVals.glId());
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, g_OutPortsValues.sizeOfData(), g_OutPortsValues.raw());
    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, 0);
    g_RBCycle = 0;
  }

  return g_RBCycle == 0;
}

/* -------------------------------------------------------- */

void simulPrintOutput_gpu(const vector<pair<string, int> >& outbits)
{
  // display result (assumes readback done)
  int val = 0;
  string str;
  for (int b = 0; b < outbits.size(); b++) {
    int vb = g_OutPortsValues[b];
    str = (vb ? "1" : "0") + str;
    val += vb << b;
  }
  fprintf(stderr, "b%s (d%d h%x)\n", str.c_str(), val, val);
}

// --------------------------------------------------------------

void simulTerminate_gpu()
{
  g_LUTs_Addrs.terminate();
  g_LUTs_Cfg.terminate();
  g_LUTs_Outputs.terminate();
  g_GPU_OutPortsLocs.terminate();
  g_GPU_OutPortsVals.terminate();
  g_GPU_OutInits.terminate();
}

// --------------------------------------------------------------
