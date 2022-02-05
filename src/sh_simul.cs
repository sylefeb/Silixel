// @sylefeb 2021-01-04
#version 430

layout(local_size_x = 128, local_size_y = 1) in;

readonly layout(std430, binding = 0) buffer Buf0 { uint  cfg    []; };
readonly layout(std430, binding = 1) buffer Buf1 { ivec4 addrs  []; };
coherent layout(std430, binding = 2) buffer Buf2 { uint  outputs[]; };

uniform uint start_lut;
uniform uint num;

uint get_output(uint a)
{
  return (outputs[a >> 1u] >> (a & 1u)) & 1u;
}

void main()
{
  if (gl_GlobalInvocationID.x < num)
  {
    uint lut_id = start_lut + gl_GlobalInvocationID.x;
    // apply LUT logic
    uint C  = cfg  [lut_id];
    ivec4 a = addrs[lut_id];
    uint i0 = get_output(a.x);
    uint i1 = get_output(a.y);
    uint i2 = get_output(a.z);
    uint i3 = get_output(a.w);
    uint sh = i3 | (i2 << 1) | (i1 << 2) | (i0 << 3);
    if (((C >> sh) & 1u) == 1u) {
      atomicOr (outputs[lut_id], 0x00000001u);
    } else {
      atomicAnd(outputs[lut_id], 0xfffffffeu);
    }
  }
}
