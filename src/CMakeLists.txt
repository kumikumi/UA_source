cmake_minimum_required(VERSION 3.5)
project(UA_source)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 14)

set(SOURCES 
	3ds.cpp
	ade.cpp
	area.cpp
	amesh.cpp
	bitmap.cpp
	base.cpp
	benchmark.cpp
	bmpAnm.cpp
	button.cpp
	crc32.cpp
	def_parser.cpp
	embed.cpp
	env.cpp
	fmtlib/format.cc
	font.cpp
	idev.cpp
	ilbm.cpp
	image.cpp
	ini.cpp
	itimer.cpp
	iwimp.cpp
	log.cpp
	loaders.cpp
	locale/default.cpp
	locale/locale.cpp
	lstvw.cpp
	main.cpp
	IFFile.cpp
	net/session.cpp
	net/viewclient.cpp
	net/viewserver.cpp
	network.cpp
	nucleas.cpp
	obj3d.cpp
	particle.cpp
	rsrc.cpp
	sample.cpp
	skeleton.cpp
	sklt.cpp
	system/cmdline.cpp
	system/fsmgr.cpp
	system/gfx.cpp
	system/gl_backend.cpp
	system/glfuncs.cpp
	system/inivals.cpp
	system/inpt.cpp
	system/movie.cpp
	system/null_backend.cpp
	system/perf.cpp
	system/sound.cpp
	system/stdshdr.cpp
	system/system.cpp
	system/tform.cpp
	utils.cpp
	wav.cpp
	windp.cpp
	winp.cpp
	wintimer.cpp
	world/analyzer.cpp
	world/frame.cpp
	world/history.cpp
	world/parsers.cpp
	world/nparticle.cpp
	world/playback.cpp
	world/playercontrols.cpp
	world/playerorder.cpp
	world/protos.cpp
	world/recorder.cpp
	world/viewstream.cpp
	world/state.cpp
	world/tools.cpp
	world/tutorialhints.cpp
	wrapal.cpp
	yw.cpp
	yw_brif.cpp
	yw_funcs.cpp
	yw_viewdist.cpp
	yw_func2.cpp
	yw_game.cpp
	yw_game_ui.cpp
	yw_net.cpp
	yw_replay.cpp
	yw_replayui.cpp
	yw_replays.cpp
	yw_save.cpp
	ypabact.cpp
	ypacar.cpp
	ypaflyer.cpp
	ypagun.cpp
	ypamissile.cpp
	yparobo.cpp
	ypatank.cpp
	ypaufo.cpp
)


find_package(PkgConfig REQUIRED)

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

pkg_search_module(VORBISFILE REQUIRED vorbisfile)
link_directories(${VORBISFILE_LIBRARY_DIRS})

# Only the components used. Modern FFmpeg no longer ships libavresample or
# libpostproc, which FindFFmpeg would otherwise require by default.
find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil swscale swresample)

find_package(OpenGL REQUIRED)

if(APPLE)
    # Apple's OpenAL framework is deprecated; use OpenAL Soft instead. Homebrew
    # installs it keg-only, so the macos preset adds it to CMAKE_PREFIX_PATH.
    set(CMAKE_FIND_FRAMEWORK LAST)
    find_package(OpenAL REQUIRED)
    set(CMAKE_FIND_FRAMEWORK FIRST)
    if(OPENAL_INCLUDE_DIR MATCHES "\\.framework")
        message(FATAL_ERROR "OpenAL Soft not found. Run: brew install openal-soft, "
                            "then configure with: cmake --preset macos")
    endif()
else()
    find_package(OpenAL REQUIRED)
endif()

# Recordings compress their blocks with zlib
find_package(ZLIB REQUIRED)

# Network games run on ENet 1.3.18 (enet/, MIT licence, from
# github.com/lsalzman/enet), built here so no system package is needed
add_library(enet STATIC
	enet/callbacks.c
	enet/compress.c
	enet/host.c
	enet/list.c
	enet/packet.c
	enet/peer.c
	enet/protocol.c
	enet/unix.c
	enet/win32.c)
target_include_directories(enet PUBLIC enet/include)

if(WIN32)
	target_link_libraries(enet PUBLIC ws2_32 winmm)
elseif(NOT APPLE)
	# What ENet's own build finds on any current Linux or BSD; on macOS its
	# unix.c sets these itself
	target_compile_definitions(enet PRIVATE HAS_POLL=1 HAS_FCNTL=1 HAS_INET_PTON=1 HAS_INET_NTOP=1
		HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1)
endif()

# The SDL_GPU renderer's shaders are written in Slang (src/shaders) and
# compiled when the game is built: to Metal Shading Language for macOS and to
# SPIR-V for Vulkan. Without slangc, only the OpenGL renderer is built.
option(UA_GPU_RENDERER "Build the SDL_GPU renderer (needs slangc)" ON)

if(UA_GPU_RENDERER)
    find_program(SLANGC slangc HINTS "$ENV{HOME}/.local/bin")

    if(NOT SLANGC)
        message(WARNING "slangc not found: building without the SDL_GPU renderer")
        set(UA_GPU_RENDERER OFF)
    endif()
endif()

if(UA_GPU_RENDERER)
    set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

    set(SHADER_SOURCES
        ${SHADER_SRC_DIR}/common.slang
        ${SHADER_SRC_DIR}/composite.slang
        ${SHADER_SRC_DIR}/forcefield.slang
        ${SHADER_SRC_DIR}/world.slang)

    # Entry point, its file and its stage
    set(SHADER_ENTRIES
        vsWorld          world     vertex
        fsWorld          world     fragment
        fsWorldAlphaTest world     fragment
        vsQuad           composite vertex
        fsQuad           composite fragment
        fsColorEffects   composite fragment
        fsForceField     forcefield fragment)

    set(SHADER_OUTPUTS "")
    set(SHADER_NAMES "")
    list(LENGTH SHADER_ENTRIES count)
    math(EXPR last "${count} - 1")

    foreach(i RANGE 0 ${last} 3)
        math(EXPR j "${i} + 1")
        math(EXPR k "${i} + 2")
        list(GET SHADER_ENTRIES ${i} entry)
        list(GET SHADER_ENTRIES ${j} file)
        list(GET SHADER_ENTRIES ${k} stage)

        # SPIR-V 1.0 works with any Vulkan driver; slangc warns (E50011) that
        # it is old, but the modules validate for Vulkan 1.0.
        set(options ${SHADER_SRC_DIR}/${file}.slang -entry ${entry} -stage ${stage}
                    -line-directive-mode none -warnings-disable 50011)

        add_custom_command(
            OUTPUT ${SHADER_OUT_DIR}/${entry}.metal ${SHADER_OUT_DIR}/${entry}.spv
            COMMAND ${SLANGC} ${options} -target metal -o ${SHADER_OUT_DIR}/${entry}.metal
            COMMAND ${SLANGC} ${options} -target spirv -profile spirv_1_0 -o ${SHADER_OUT_DIR}/${entry}.spv
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Compiling shader ${entry}"
            VERBATIM)

        list(APPEND SHADER_OUTPUTS ${SHADER_OUT_DIR}/${entry}.metal ${SHADER_OUT_DIR}/${entry}.spv)
        list(APPEND SHADER_NAMES ${entry})
    endforeach()

    string(REPLACE ";" "," SHADER_NAMES "${SHADER_NAMES}")

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gpu_shaders.cpp
        COMMAND ${CMAKE_COMMAND} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/gpu_shaders.cpp
                -DSHADER_DIR=${SHADER_OUT_DIR} -DENTRIES=${SHADER_NAMES}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaders.cmake
        DEPENDS ${SHADER_OUTPUTS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaders.cmake
        COMMENT "Embedding the shaders"
        VERBATIM)

    list(APPEND SOURCES system/gpu_backend.cpp ${CMAKE_CURRENT_BINARY_DIR}/gpu_shaders.cpp)
endif()


if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    set (WIN32_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../svg/icon/icon.rc")
endif()


if (WIN32)
  add_executable(UA_source WIN32
                       ${SOURCES}
                       ${WIN32_RESOURCES}
                )
else()
  add_executable(UA_source ${SOURCES})
endif()




target_link_libraries(UA_source SDL3::SDL3
							 SDL3_image::SDL3_image
							 SDL3_ttf::SDL3_ttf
							 enet
							 ${OPENGL_LIBRARIES}
							 ${VORBISFILE_LIBRARIES}
							 ${OPENAL_LIBRARY}
							 ZLIB::ZLIB
							 FFmpeg::avcodec
							 FFmpeg::avformat
							 FFmpeg::avutil
							 FFmpeg::swscale
							 FFmpeg::swresample)

target_include_directories(UA_source PUBLIC
										${PROJECT_SOURCE_DIR} 
										${OpenGL_INCLUDE_DIR}
										${VORBISFILE_INCLUDE_DIRS}
										${OPENAL_INCLUDE_DIR})

if(UA_GPU_RENDERER)
    target_compile_definitions(UA_source PRIVATE UA_GPU_RENDERER)

    if(APPLE)
        # For setting up the window's Metal layer through the Objective-C runtime
        target_link_libraries(UA_source objc)
    endif()
endif()

if(APPLE)
    # Every OpenGL call is marked deprecated on macOS; the warnings bury real ones.
    target_compile_definitions(UA_source PRIVATE GL_SILENCE_DEPRECATION)
endif()

if(CMAKE_BUILD_TYPE MATCHES [dD][eE][bB][uU][gG])
   target_compile_options(UA_source PUBLIC "-g" "-O0")
   
   if(NOT WIN32)
      target_compile_options(UA_source PUBLIC "-fsanitize=address")
      target_link_libraries(UA_source "-fsanitize=address")
   endif()
else()
	target_compile_options(UA_source PUBLIC "-g" "-O0")
	#target_compile_options(UA_source PUBLIC "-O2" "-s")
endif()

