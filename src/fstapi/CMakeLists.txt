cmake_minimum_required(VERSION 3.5)
project(fstapi)

INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}/
  ${PROJECT_SOURCE_DIR}/../LibSL/src/libs/src/zlib/
)

ADD_LIBRARY(fstapi
  fastlz.c
  fastlz.h
  lz4.c
  lz4.h
  fstapi.c
  fstapi.h
)
