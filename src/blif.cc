// @sylefeb 2022-01-08
/*

Simple BLIF file parser, nothing special.
Reads the inputs, outputs, gates and latches into a t_blif struct.

*/
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

#include "blif.h"
#include <LibSL/LibSL.h>

// -------------------------------------------------------------------

using namespace std;

// -------------------------------------------------------------------

void readList(
  LibSL::BasicParser::Parser<LibSL::BasicParser::FileStream>& parser,
  vector<string>& _list)
{
  while (1) {
    parser.skipSpaces();
    char next = parser.readChar(false);
    if (next == '\n') {
      break;
    } else {
      string name = parser.readString();
      _list.emplace_back(name);
    }
  }
}

// -------------------------------------------------------------------

void readConfig(
  LibSL::BasicParser::Parser<LibSL::BasicParser::FileStream>& parser,
  pair<string,string>& _cfgs)
{
  string vals = parser.readString();
  string out  = parser.readString();
  if (out.empty()) { std::swap(vals,out); }
  _cfgs = make_pair(vals, out);
}

// -------------------------------------------------------------------

ushort lut_config(const std::vector<std::pair<std::string, std::string> >& config_strings)
{
  ushort cfg = 0;
  for (auto cs : config_strings) {
    if (cs.second == "1") { // probably always the case, defaults to 0
      ForIndex(c, 16) { // for each of 16 configs
        bool accept = true;
        ForIndex(j, cs.first.length()) {
          if (cs.first[cs.first.length()-1-j] == '1') {
            if (!(c & (1 << j))) { accept = false; break; }
          } else {
            if (  c & (1 << j) ) { accept = false; break; }
          }
        }
        if (accept) {
          cfg |= (1 << c);
        }
      }
    }
  }
  return cfg;
}

// -------------------------------------------------------------------

void parse(const char *fname, t_blif& _blif)
{

  LibSL::BasicParser::FileStream stream(fname);
  LibSL::BasicParser::Parser<LibSL::BasicParser::FileStream> parser(stream,false);

  fprintf(stderr, " Parsing ... ");
  Console::processingInit();
  while (!parser.eof()) {
    parser.skipSpaces();
    char first = parser.readChar(false);
    if (first == '#') {
      // skip comment
    } else if (first == '.') {
      string type = parser.readString();
      if (type == ".model") {
        string name = parser.readString();
      } else if (type == ".inputs") {
        readList(parser, _blif.inputs);
      } else if (type == ".outputs") {
        readList(parser, _blif.outputs);
      } else if (type == ".names") {
        vector<string> ios;
        readList(parser, ios);
        _blif.gates.push_back(t_gate_nfo());
        if (!ios.empty()) {
          _blif.gates.back().output = ios.back();
          for (int i = 0; i < (int)ios.size() - 1; ++i) {
            _blif.gates.back().inputs.push_back(ios[i]);
          }
        }
      } else if (type == ".latch") {
        _blif.latches.push_back(t_latch_nfo());
        vector<string> nfos;
        readList(parser, nfos);
        sl_assert(nfos.size() == 5);
        _blif.latches.back().input  = nfos[0];
        _blif.latches.back().output = nfos[1];
        _blif.latches.back().init = nfos[4];
      }
    } else if (first == '0' || first == '1' || first == '-') {
      // read configuration
      _blif.gates.back().config_strings.push_back(pair<string, string>());
      readConfig(parser, _blif.gates.back().config_strings.back());
    }
    // skip to next line
    parser.reachChar('\n');
    Console::processingUpdate();
  }
  Console::processingEnd();
  fprintf(stderr, " done.\n");

}

// -------------------------------------------------------------------
