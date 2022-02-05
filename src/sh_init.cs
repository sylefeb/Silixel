// @sylefeb 2021-01-04
#version 430

layout(local_size_x = 1, local_size_y = 1) in;

coherent layout(std430, binding = 2) buffer Buf2 { uint outputs[]; };
readonly layout(std430, binding = 5) buffer Buf5 { uint ones   []; };

void main()
{
  uint id        = gl_GlobalInvocationID.x;
  // update flipflop
  uint o         = ones[id];
  atomicOr(outputs[o >> 1u], 1u << (o & 1u));
}
