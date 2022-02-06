CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

ADD_SUBDIRECTORY(LibSL)

# ADD_DEFINITIONS(-DUSE_GLUX -DOPENGL -DOPENGL4)

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
	simul.cc
	simul.h
	)

TARGET_LINK_LIBRARIES(silixel LibSL LibSL_gl4core)

ADD_DEFINITIONS(-DSRC_PATH=\"${CMAKE_SOURCE_DIR}\")

ADD_EXECUTABLE(silixel-cpu
	simul_cpu.cc
	simul.cc
	simul.h
	blif.cc
	blif.h
	)

TARGET_LINK_LIBRARIES(silixel-cpu LibSL)

# IF(WIN32)
# INSTALL(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/release/silixel.exe" DESTINATION ${CMAKE_SOURCE_DIR}/bin)
# ELSE(WIN32)
# INSTALL(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/silixel" DESTINATION ${CMAKE_SOURCE_DIR}/bin)
# ENDIF(WIN32)
