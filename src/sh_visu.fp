// @sylefeb 2021-01-09
#version 430

in  vec2 uv;
out vec4 color;

readonly layout(std430, binding = 2) buffer Buf2 { uint outputs[]; };

uniform int sqsz;
uniform int num;
uniform int depth0_end;

void main()
{
  int id   = depth0_end + int(uv.x*sqsz) + int(uv.y*sqsz)*sqsz;
  ivec2  o = id < num ? ivec2(outputs[id]&1u,(outputs[id]>>1u)&1u) : ivec2(0,0);
  vec2  c  = vec2(o.xy);
  color    = vec4(c.x,c.yy,1.0);
}
