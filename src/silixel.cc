// @sylefeb 2022-01-04
// --------------------------------------------------------------

#include <iostream>
#include <ctime>
#include <cmath>

#include "simul.h"

// --------------------------------------------------------------

#include <LibSL/LibSL_gl4core.h>
#include <imgui.h>
#include <LibSL/UIHelpers/BindImGui.h>

#include "sh_simul.h"
AutoBindShader::sh_simul      g_ShSimul;

#include "sh_posedge.h"
AutoBindShader::sh_posedge    g_ShPosEdge;

#include "sh_outports.h"
AutoBindShader::sh_outports   g_ShOutPorts;

#include "sh_init.h"
AutoBindShader::sh_init       g_ShInit;

#include "sh_visu.h"
AutoBindShader::sh_visu       g_ShVisu;

typedef GPUMESH_MVF2(mvf_vertex_2f, mvf_texcoord0_2f) mvf_simple;
typedef GPUMesh_GL_VBO<mvf_simple>  GLMesh;

AutoPtr<GLMesh>               g_Quad;

// --------------------------------------------------------------

using namespace std;

// --------------------------------------------------------------

#define SCREEN_W   (640) // screen width and height
#define SCREEN_H   (480)

#define CYCLE_BUFFER_LEN 1024

// --------------------------------------------------------------

GLBuffer g_LUTs_Cfg;         // uint,  one  per LUT (NOTE: 16 bits are used, could pack)
GLBuffer g_LUTs_Addrs;       // uint,  four per LUT (NOTE: 24 bits per addr is enough, could pack on three)
GLBuffer g_LUTs_Outputs;     // uint,  one per LUT, bit 0 (D) bit 1 (Q) bit 2 (dirty)
GLBuffer g_GPU_OutPortsLocs; // uint,  per outport
GLBuffer g_GPU_OutPortsVals; // uint,  per outport * CYCLE_BUFFER_LEN (NOTE: total overkill, could be one bit)
GLBuffer g_GPU_OutInits;     // uint, one per output to initialize

GLTimer  g_GPU_timer;

// --------------------------------------------------------------

map<string, v2i> g_OutPorts;
Array<int>       g_OutPortsValues;

vector<int>      g_step_starts;
vector<int>      g_step_ends;
vector<t_lut>    g_luts;
vector<int>      g_ones;
vector<int>      g_cpu_fanout;
vector<uchar>    g_cpu_depths;
vector<uchar>    g_cpu_outputs;
vector<int>      g_cpu_computelists;

bool             g_Use_GPU = true;

// --------------------------------------------------------------

bool designHasVGA()
{
  return (g_OutPorts.count("out_video_vs") > 0);
}

// --------------------------------------------------------------
const int G = 128;
// --------------------------------------------------------------

void initialize(const vector<t_lut>& luts,const vector<int>& ones)
{
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

int g_Cycle = 0;

void simulCycle_gpu(
  const vector<t_lut>& luts,
  const vector<int>&   step_starts,
  const vector<int>&   step_ends)
{

  g_ShSimul.begin();
  
  ForRange(depth, 1, (int)step_starts.size()-1) {
    int n = step_ends[depth] - step_starts[depth] + 1;
    g_ShSimul.start_lut.set((uint)step_starts[depth]);
    g_ShSimul.num.set((uint)n);
    g_ShSimul.run(v3i((n / G) + ((n & (G - 1)) ? 1 : 0), 1, 1));
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }

  g_ShSimul.end();

  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  {
    g_ShPosEdge.begin();
    int n = (int)luts.size();
    g_ShPosEdge.num.set((uint)n);
    g_ShPosEdge.run(v3i((n / G) + ((n & (G - 1)) ? 1 : 0), 1, 1));
    g_ShPosEdge.end();
  }

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
    SimpleUI::onRender = mainRender;
    SimpleUI::init(SCREEN_W, SCREEN_H);
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
    g_ShSimul.init();
    g_ShPosEdge.init();
    g_ShOutPorts.init();
    g_ShInit.init();
    g_ShVisu.init();

    /// GPU timer
    g_GPU_timer.init();

    /// load up design
    vector<t_lut> init_luts;
    vector<pair<string,int> > outbits;
    readDesign(init_luts, outbits, g_ones);

    vector<int>   reorder;
    vector<int>   inv_reorder;
    analyze(init_luts, g_ones, reorder, inv_reorder, g_step_starts, g_step_ends, g_cpu_depths);

    reorderLUTs(init_luts, reorder, inv_reorder, g_luts, outbits, g_ones);

    buildFanout(g_luts, g_cpu_fanout);

    int rank = 0;
    for (auto op : outbits) {
      g_OutPorts.insert(make_pair(op.first,v2i(rank++, op.second)));
    }
    g_OutPortsValues.allocate(rank * CYCLE_BUFFER_LEN);

    /// GPU buffers init
    initialize(g_luts, g_ones);

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
    g_ShSimul.terminate();
    g_ShPosEdge.terminate();
    g_ShOutPorts.terminate();
    g_ShInit.terminate();
    g_ShVisu.terminate();
    g_LUTs_Addrs.terminate();
    g_LUTs_Cfg.terminate();
    g_LUTs_Outputs.terminate();
    g_GPU_OutPortsLocs.terminate();
    g_GPU_OutPortsVals.terminate();
    g_GPU_OutInits.terminate();
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
