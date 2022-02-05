// @sylefeb 2021-01-04
#version 430

layout(local_size_x = 128, local_size_y = 1) in;

coherent layout(std430, binding = 2) buffer Buf2 { uint outputs[]; };

uniform uint num;

void main()
{
  if (gl_GlobalInvocationID.x < num)
  {
    uint lut_id = gl_GlobalInvocationID.x;
    // update Q output from D
    uint outv = outputs[lut_id];
    if ((outv & 1u) == 1u) {
      outputs[lut_id] = 3u;
    } else {
      outputs[lut_id] = 0u;
    }
  }
}
