CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

ADD_SUBDIRECTORY(LibSL EXCLUDE_FROM_ALL)
ADD_SUBDIRECTORY(fstapi)

# ADD_DEFINITIONS(-DUSE_GLUX -DOPENGL -DOPENGL4)

IF(WASI)
ELSE()

SET(SHADERS
	sh_simul
	sh_posedge
	sh_outports
	sh_init
	sh_visu
)
AUTO_BIND_SHADERS( ${SHADERS} )

ADD_EXECUTABLE(silixel
	silixel.cc
	blif.cc
	blif.h
	sh_simul.cs
	sh_simul.h
	sh_posedge.cs
	sh_posedge.h
	sh_outports.cs
	sh_outports.h
	sh_init.cs
	sh_init.h
	sh_visu.fp
	sh_visu.vp
	sh_visu.h
	simul_cpu.cc
	simul_cpu.h
	simul_gpu.cc
	simul_gpu.h
	read.cc
	read.h
	analyze.cc
	analyze.h
	)

IF(LINUX)
TARGET_LINK_LIBRARIES(silixel LibSL LibSL_gl4core freeglut)
ELSE()
TARGET_LINK_LIBRARIES(silixel LibSL LibSL_gl4core)
ENDIF()

ENDIF()

ADD_DEFINITIONS(-DSRC_PATH=\"${CMAKE_SOURCE_DIR}\")

ADD_EXECUTABLE(silixel_cpu
	silixel_cpu.cc
	simul_cpu.cc
	simul_cpu.h
	blif.cc
	blif.h
	read.cc
	read.h
	analyze.cc
	analyze.h
	)

TARGET_LINK_LIBRARIES(silixel_cpu LibSL fstapi)

# install and paths

install(TARGETS silixel_cpu  RUNTIME  DESTINATION bin/)
