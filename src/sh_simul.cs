// @sylefeb 2021-01-04
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

    uint outv = outputs[lut_id];
    uint old_d = outv & 1u;
    uint new_d = (C >> sh) & 1u;

    if (old_d != new_d) {
      if (new_d == 1u){
        outputs[lut_id] = outv | 1u;
      }
      else{
        outputs[lut_id] = outv & 0xfffffffeu;
      }
    }
  }
}
