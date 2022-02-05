// @sylefeb 2022-01-08
#pragma once

#include <LibSL/LibSL.h>

#include <vector>
#include <string>

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
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
  std::vector<t_latch_nfo> latches;
  std::vector<t_gate_nfo>  gates;
} t_blif;

void   parse(const char *fname, t_blif& _blif);
ushort lut_config(const std::vector<std::pair<std::string, std::string> >& config_strings);
