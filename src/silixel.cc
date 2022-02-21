// @sylefeb 2022-01-04
/* ---------------------------------------------------------------------

Main file, creates a small graphical GUI (OpenGL+ImGUI) around a 
simulated design. If the design has VGA signals, displays the result 
using a texture. Allows to select between GPU/CPU simulation.

 ----------------------------------------------------------------------- */
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

#include <iostream>
#include <ctime>
#include <cmath>

#include <LibSL/LibSL.h>
#include <LibSL/LibSL_gl4core.h>

#include "read.h"
#include "analyze.h"
#include "simul_cpu.h"
#include "simul_gpu.h"

// --------------------------------------------------------------

#include <imgui.h>
#include <LibSL/UIHelpers/BindImGui.h>

// --------------------------------------------------------------

using namespace std;

// --------------------------------------------------------------

#define SCREEN_W   (640) // screen width and height
#define SCREEN_H   (480)

// --------------------------------------------------------------

// Shader to visualize LUT outputs
#include "sh_visu.h"
AutoBindShader::sh_visu g_ShVisu;

// Output ports
map<string, v2i> g_OutPorts; // name, LUT id and rank in g_OutPortsValues
Array<int>       g_OutPortsValues; // output port values

int              g_Cycle = 0;

vector<int>      g_step_starts;
vector<int>      g_step_ends;
vector<t_lut>    g_luts;
vector<int>      g_ones;
vector<int>      g_cpu_fanout;
vector<uchar>    g_cpu_depths;
vector<uchar>    g_cpu_outputs;
vector<int>      g_cpu_computelists;

AutoPtr<GLMesh>  g_Quad;
GLTimer          g_GPU_timer;

bool             g_Use_GPU = true;

// --------------------------------------------------------------

bool designHasVGA()
{
  return (g_OutPorts.count("out_video_vs") > 0);
}

/* -------------------------------------------------------- */

ImageRGBA_Ptr g_Framebuffer;
Tex2DRGBA_Ptr g_FramebufferTex;
int    g_X  = 0;
int    g_Y  = 0;
int    g_HS = 0;
int    g_VS = 0;
double g_Hz = 0;
double g_UsecPerCycle = 0;
string g_OutPortString;
int    g_OutportCycle = 0;

/* -------------------------------------------------------- */

void simulGPUNextWait()
{
  g_GPU_timer.start();
  while (1) {
    simulCycle_gpu(g_luts, g_step_starts, g_step_ends);
    if (simulReadback_gpu()) break;
  }
  g_GPU_timer.stop();
  auto ms = g_GPU_timer.waitResult();
  g_Hz = (double)CYCLE_BUFFER_LEN / ((double)ms / 1000.0);
  g_UsecPerCycle = (double)ms * 1000.0 / (double)CYCLE_BUFFER_LEN;
  g_OutportCycle = 0;
}

/* -------------------------------------------------------- */

void simulGPUNext()
{
  g_GPU_timer.start();

  simulCycle_gpu(g_luts, g_step_starts, g_step_ends);
  bool datain = simulReadback_gpu();

  g_GPU_timer.stop();
  auto ms = g_GPU_timer.waitResult();
  g_Hz = (double)1 / ((double)ms / 1000.0);
  g_UsecPerCycle = (double)ms * 1000.0 / (double)1;

  if (datain) {
    g_OutportCycle = 0;
  } else {
    ++g_OutportCycle;
  }

}

/* -------------------------------------------------------- */

void updateFrame(int vs, int hs, int r, int g, int b)
{
  if (vs) {
    if (hs) {
      if (g_X >= 48 && g_Y >= 34) {
        g_Framebuffer->pixel<Clamp>(g_X - 48, g_Y - 34) = v4b(r << 2, g << 2, b << 2, 255);
      }
      ++g_X;
    } else {
      g_X = 0;
      if (g_HS) {
        ++g_Y;
        g_FramebufferTex = Tex2DRGBA_Ptr(new Tex2DRGBA(g_Framebuffer->pixels()));
      }
    }
  } else {
    g_X = g_Y = 0;
  }
  g_VS = vs;
  g_HS = hs;
}

/* -------------------------------------------------------- */

void simulGPU()
{
  if (designHasVGA()) { // design has VGA output, display it
    simulGPUNextWait(); // simulates a number of cycles and wait
    // read the output of the simulated cycles
    ForIndex(cy, CYCLE_BUFFER_LEN) {
      int offset = cy * (int)g_OutPorts.size();
      int vs = g_OutPortsValues[offset + g_OutPorts["out_video_vs"][0]];
      int hs = g_OutPortsValues[offset + g_OutPorts["out_video_hs"][0]];
      int r  = 0;
      ForIndex(i, 6) {
        r = r | ((g_OutPortsValues[offset + g_OutPorts["out_video_r[" + to_string(i) + "]"][0]]) << i);
      }
      int g = 0;
      ForIndex(i, 6) {
        g = g | ((g_OutPortsValues[offset + g_OutPorts["out_video_g[" + to_string(i) + "]"][0]]) << i);
      }
      int b = 0;
      ForIndex(i, 6) {
        b = b | ((g_OutPortsValues[offset + g_OutPorts["out_video_b[" + to_string(i) + "]"][0]]) << i);
      }
      updateFrame(vs, hs, r, g, b);
    }
  } else { // design has no VGA, show the output ports
    simulGPUNext(); // step one cycle
    // make the output string
    g_OutPortString = "";
    int offset = g_OutportCycle * (int)g_OutPorts.size();
    for (auto op : g_OutPorts) {
      g_OutPortString = (g_OutPortsValues[offset + op.second[0]] ? "1" : "0") + g_OutPortString;
    }
  }
}

/* -------------------------------------------------------- */

uchar simulCPU_output(std::string o)
{
  int pos      = g_OutPorts.at(o)[1];
  int lut      = pos >> 1;
  int q_else_d = pos & 1;
  uchar bit = (g_cpu_outputs[lut] >> q_else_d) & 1;
  return bit;
}

/* -------------------------------------------------------- */

void simulCPU()
{
  if (designHasVGA()) {
    // multiple steps
    int num_measures = 0;
    Elapsed el;
    while (num_measures++ < 100) {
      simulCycle_cpu(g_luts, g_cpu_depths, g_step_starts, g_step_ends, g_cpu_fanout, g_cpu_computelists, g_cpu_outputs);
      simulPosEdge_cpu(g_luts, g_cpu_depths, (int)g_step_starts.size(), g_cpu_fanout, g_cpu_computelists, g_cpu_outputs);
      int vs = simulCPU_output("out_video_vs");
      int hs = simulCPU_output("out_video_hs");
      int r = 0;
      ForIndex(i, 6) {
        r = r | (simulCPU_output("out_video_r[" + to_string(i) + "]") << i);
      }
      int g = 0;
      ForIndex(i, 6) {
        g = g | (simulCPU_output("out_video_g[" + to_string(i) + "]") << i);
      }
      int b = 0;
      ForIndex(i, 6) {
        b = b | (simulCPU_output("out_video_b[" + to_string(i) + "]") << i);
      }
      updateFrame(vs, hs, r, g, b);
    }
    auto ms = el.elapsed();
    g_Hz = (double)100 / ((double)ms / 1000.0);
    g_UsecPerCycle = (double)ms * 1000.0 / (double)100;
  } else {
    // step
    Elapsed el;
    simulCycle_cpu(g_luts, g_cpu_depths, g_step_starts, g_step_ends, g_cpu_fanout, g_cpu_computelists, g_cpu_outputs);
    simulPosEdge_cpu(g_luts, g_cpu_depths, (int)g_step_starts.size(), g_cpu_fanout, g_cpu_computelists, g_cpu_outputs);
    auto ms = el.elapsed();
    if (ms > 0) {
      g_Hz = (double)100 / ((double)ms / 1000.0);
      g_UsecPerCycle = (double)ms * 1000.0 / (double)100;
    } else {
      g_Hz = -1;
      g_UsecPerCycle = -1;
    }
    // make the output string
    g_OutPortString = "";
    for (auto op : g_OutPorts) {
      g_OutPortString = (simulCPU_output(op.first) ? "1" : "0") + g_OutPortString;
    }
  }
}

/* -------------------------------------------------------- */

void mainRender()
{

  // simulate
  if (g_Use_GPU) {
    simulGPU();
  } else {
    simulCPU();
  }

  // basic rendering
  LibSL::GPUHelpers::clearScreen(LIBSL_COLOR_BUFFER | LIBSL_DEPTH_BUFFER, 0.2f, 0.2f, 0.2f);

  // render display
  if (designHasVGA()) {
    // -> texture for VGA display
    GLBasicPipeline::getUniqueInstance()->begin();
    GLBasicPipeline::getUniqueInstance()->setProjection(orthoMatrixGL<float>(0, 1, 1, 0, -1, 1));
    GLBasicPipeline::getUniqueInstance()->setModelview(m4x4f::identity());
    GLBasicPipeline::getUniqueInstance()->setColor(v4f(1));
    if (!g_FramebufferTex.isNull()) {
      g_FramebufferTex->bind();
    }
    GLBasicPipeline::getUniqueInstance()->enableTexture();
    GLBasicPipeline::getUniqueInstance()->bindTextureUnit(0);
    g_Quad->render();
    GLBasicPipeline::getUniqueInstance()->end();
  }

  // render LUTs+FF
  if (g_Use_GPU) {
    GLProtectViewport vp;
    glViewport(0, 0, SCREEN_H*2/3, SCREEN_H*2/3);
    g_ShVisu.begin();
    g_Quad->render();
    g_ShVisu.end();
  }

  // -> GUI
  ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Once);
  ImGui::Begin("Status");
  ImGui::Checkbox("Simulate on GPU", &g_Use_GPU);
  ImGui::Text("%5.1f KHz %5.1f usec / cycle", g_Hz/1000.0, g_UsecPerCycle);
  ImGui::Text("simulated cycle: %6d", g_Cycle);
  ImGui::Text("simulated LUT4+FF %7d", g_luts.size());
  ImGui::Text("screen row %3d",g_Y);
  if (!g_OutPortString.empty()) {
    ImGui::Text("outputs: %s", g_OutPortString.c_str());
  }
  ImGui::End();

  ImGui::Render();
}

/* -------------------------------------------------------- */

int main(int argc, char **argv)
{
  try {

    /// init simple UI (glut clone for both GL and D3D)
    cerr << "Init SimpleUI   ";
    SimpleUI::init(SCREEN_W, SCREEN_H);
    SimpleUI::onRender = mainRender;
    cerr << "[OK]" << endl;

    /// bind imgui
    SimpleUI::bindImGui();
    SimpleUI::initImGui();
    SimpleUI::onReshape(SCREEN_W, SCREEN_H);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    /// help
    printf("[ESC]    - quit\n");

    /// display stuff
    g_Framebuffer = ImageRGBA_Ptr(new ImageRGBA(640,480));
    g_Quad = AutoPtr<GLMesh>(new GLMesh());
    g_Quad->begin(GPUMESH_TRIANGLESTRIP);
    g_Quad->texcoord0_2(0, 0); g_Quad->vertex_2(0, 0);
    g_Quad->texcoord0_2(1, 0); g_Quad->vertex_2(1, 0);
    g_Quad->texcoord0_2(0, 1); g_Quad->vertex_2(0, 1);
    g_Quad->texcoord0_2(1, 1); g_Quad->vertex_2(1, 1);
    g_Quad->end();

    /// GPU shaders init
    g_ShVisu.init();

    /// GPU timer
    g_GPU_timer.init();

    /// load up design
    vector<pair<string,int> > outbits;
    readDesign(g_luts, outbits, g_ones);

    analyze(g_luts, outbits, g_ones, g_step_starts, g_step_ends, g_cpu_depths);

    buildFanout(g_luts, g_cpu_fanout);

    int rank = 0;
    for (auto op : outbits) {
      g_OutPorts.insert(make_pair(op.first,v2i(rank++, op.second)));
    }
    g_OutPortsValues.allocate(rank * CYCLE_BUFFER_LEN);

    /// GPU buffers init
    simulInit_gpu(g_luts, g_ones);

    // init CPU simulation
    simulInit_cpu(g_luts, g_step_starts, g_step_ends, g_ones, g_cpu_computelists, g_cpu_outputs);

    /// Quick benchmarking at startup
#if 1
    // -> time GPU
    simulBegin_gpu(g_luts,g_step_starts,g_step_ends,g_ones);
    {
      ForIndex(trials, 3) {
        int n_cycles = 10000;
        g_GPU_timer.start();
        ForIndex(cycle, n_cycles) {
          simulCycle_gpu(g_luts, g_step_starts, g_step_ends);
          simulReadback_gpu();
          ++g_Cycle;
        }
        g_GPU_timer.stop();
        simulPrintOutput_gpu(outbits);
        auto ms = g_GPU_timer.waitResult();
        printf("[GPU] %d msec, ~ %f Hz, cycle time: %f usec\n",
          (int)ms,
          (double)n_cycles / ((double)ms / 1000.0),
          (double)ms * 1000.0 / (double)n_cycles);
      }
    }
    simulEnd_gpu();
    // -> time CPU
    {
      ForIndex(trials, 3) {
        Elapsed el;
        int n_cycles = 1000;
        ForIndex(cy, n_cycles) {
          simulCycle_cpu(g_luts, g_cpu_depths, g_step_starts, g_step_ends, g_cpu_fanout, g_cpu_computelists, g_cpu_outputs);
          simulPosEdge_cpu(g_luts, g_cpu_depths, (int)g_step_starts.size(), g_cpu_fanout, g_cpu_computelists, g_cpu_outputs);
        }
        auto ms = el.elapsed();
        printf("[CPU] %d msec, ~ %f Hz, cycle time: %f usec\n",
          (int)ms,
          (double)n_cycles / ((double)ms / 1000.0),
          (double)ms * 1000.0 / (double)n_cycles);
      }
    }
#endif

    /// shader parameters
    g_ShVisu.begin();
    int n_simul = (int)g_luts.size() - g_step_ends[0];
    int sqsz = (int)sqrt((double)(n_simul)) + 1;
    fprintf(stderr, "simulating %d LUTs+FF (%dx%d pixels)", n_simul, sqsz, sqsz);
    g_ShVisu.sqsz      .set(sqsz);
    g_ShVisu.num       .set((int)(g_luts.size()));
    g_ShVisu.depth0_end.set((int)(g_step_ends[0]));
    g_ShVisu.end();

    /// main loop
    simulBegin_gpu(g_luts, g_step_starts, g_step_ends, g_ones);
    SimpleUI::loop();
    simulEnd_gpu();

    /// clean exit
    simulTerminate_gpu();
    g_ShVisu.terminate();
    g_GPU_timer.terminate();
    g_FramebufferTex = Tex2DRGBA_Ptr();
    g_Quad = AutoPtr<GLMesh>();

    /// shutdown SimpleUI
    SimpleUI::shutdown();

  } catch (Fatal& e) {
    cerr << e.message() << endl;
    return (-1);
  }

  return (0);
}

/* -------------------------------------------------------- */
