// @sylefeb 2021-01-04
#version 430

layout(local_size_x = 1, local_size_y = 1) in;

coherent  readonly layout(std430, binding = 2) buffer Buf2 { uint outputs []; };
readonly  layout(std430, binding = 3) buffer Buf3 { uint portlocs[]; };
writeonly layout(std430, binding = 4) buffer Buf4 { uint portvals[]; };

uniform uint offset;

void main()
{
  uint id = gl_GlobalInvocationID.x;
  uint o  = portlocs[id];
  portvals[offset + id] = (outputs[o>>1u] >> (o&1u)) & 1u;
}
