// @sylefeb 2022-01-08
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

#pragma once

#include <LibSL/LibSL.h>

#include <vector>
#include <string>

#include "uintX.h"

typedef struct {
  std::string input;
  std::string output;
  std::string init;
} t_latch_nfo;

typedef struct {
  std::vector<std::string> inputs;
  std::string              output;
  std::vector<std::pair<std::string, std::string> > config_strings;
} t_gate_nfo;

typedef struct {
  std::map<std::string,std::string> bindings;
  std::string name;
  int   size;
  int   addr_width;
  int   data_width;
  uintX data;
} t_bram_nfo;

typedef struct {
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
  std::vector<t_latch_nfo> latches;
  std::vector<t_gate_nfo>  gates;
  std::vector<t_bram_nfo>  brams;
} t_blif;

/// Parses a blif file
void   parse(const char *fname, t_blif& _blif);

/// Returns an integer representing the LUT configuration provides as strings
ushort lut_config(const std::vector<std::pair<std::string, std::string> >& config_strings);

/// Utilities
// splits a string using a delimiter
void   split(const std::string &s, char delim, std::vector<std::string> &elems);
