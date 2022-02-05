// @sylefeb 2021-01-04
#version 430

layout(local_size_x = 1, local_size_y = 1) in;

layout(std430, binding = 0) buffer Buf { uint buf[]; };

void main()
{
  uint id = gl_GlobalInvocationID.x;
  buf[id] = 0;
}
