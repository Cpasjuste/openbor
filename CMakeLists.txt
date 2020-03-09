cmake_minimum_required(VERSION 3.0)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# delete cmake cache folder before changing this options
option(BUILD_SDL1 "Build with SDL1 support" OFF)
option(BUILD_SDL2 "Build with SDL2 support" OFF)
option(BUILD_PSP2 "Build with PSP2 support" ON)
option(BUILD_NX "Build with NX (SWITCH) support" OFF)

option(BUILD_GFX "Build with GFX support" OFF)
option(BUILD_TREMOR "Build with TREMOR support" ON)
option(BUILD_VORBIS "Build with VORBIS support" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_DEBUG true CACHE BOOL "Debug build")
endif ()

# setup toolchain
include(Toolchain.cmake)

project(OpenBOR)

##############
# COMMON
##############
set(BOR_INC
        engine
        engine/source
        engine/source/adpcmlib
        engine/source/gamelib
        engine/source/preprocessorlib
        engine/source/ramlib
        engine/source/randlib
        engine/source/scriptlib
        engine/source/openborscript
        engine/source/pnglib
        )

file(GLOB BOR_SRC
        engine/*.c
        engine/source/*.c
        engine/source/adpcmlib/*.c
        engine/source/gamelib/*.c
        engine/source/preprocessorlib/*.c
        engine/source/ramlib/*.c
        engine/source/randlib/*.c
        engine/source/scriptlib/*.c
        engine/source/openborscript/*.c
        engine/source/pnglib/*.c
        )

set(BOR_CFLAGS
        -g -Wall -fsigned-char
        -fomit-frame-pointer
        )

set(BOR_LDFLAGS
        png z m
        )

#####################
# OPTIONS
#####################
if (BUILD_GFX)
    list(APPEND BOR_INC engine/source/gfxlib)
    file(GLOB GFX_SRC engine/source/gfxlib/*.c)
    list(APPEND BOR_SRC ${GFX_SRC})
endif (BUILD_GFX)
if (BUILD_VORBIS)
    list(APPEND BOR_CFLAGS -DOV_EXCLUDE_STATIC_CALLBACKS)
    list(APPEND BOR_LDFLAGS vorbisfile vorbis ogg)
endif (BUILD_VORBIS)
if (BUILD_TREMOR)
    list(APPEND BOR_CFLAGS -DTREMOR)
    list(APPEND BOR_LDFLAGS vorbisidec ogg)
endif (BUILD_TREMOR)

#####################
# PLATORM SPECIFIC
#####################
if (BUILD_PSP2)
    #####################
    # PSP2 PLATORM
    #####################
    list(REMOVE_ITEM BOR_SRC ${CMAKE_CURRENT_SOURCE_DIR}/engine/source/gamelib/filters.c)
    list(APPEND BOR_INC engine/vita)
    file(GLOB PSP2_SRC engine/vita/*.c)
    list(APPEND BOR_SRC ${PSP2_SRC})
    list(APPEND BOR_CFLAGS
            -D__VITA__ -DVITA -D__PSP2SHELL__
            -mfpu=neon -mcpu=cortex-a9 -march=armv7-a -mfloat-abi=hard -ffast-math
            )
    link_directories(engine/vita/vita-shader-collection/lib)
    list(APPEND BOR_LDFLAGS
            vita2d vitashaders
            SceDisplay_stub SceGxm_stub ScePower_stub
            SceSysmodule_stub SceCtrl_stub ScePgf_stub
            SceCommonDialog_stub SceAudio_stub freetype jpeg m
            )
elseif (BUILD_NX)
    #####################
    # NX PLATORM
    #####################
    # Ryujinx doesn't like original png decoding code.. replace that
    list(REMOVE_ITEM BOR_SRC ${CMAKE_CURRENT_SOURCE_DIR}/engine/source/pnglib/pngdec.c)
    list(APPEND BOR_INC engine/switch)
    file(GLOB SWITCH_SRC engine/switch/*.c)
    list(APPEND BOR_SRC ${SWITCH_SRC})
    list(APPEND BOR_CFLAGS
            -DSDL -DSDL2 -DSWITCH -D__SWITCH__
            -I${DEVKITPRO}/portlibs/switch/include/SDL2
            )
    list(APPEND BOR_LDFLAGS
            SDL2 nx m
            -specs=${DEVKITPRO}/libnx/switch.specs
            )
elseif (BUILD_SDL2)
    #####################
    # SDL2 PLATORM
    #####################
elseif (BUILD_SDL1)
    #####################
    # SDL1 PLATORM
    #####################
endif (BUILD_PSP2)

#####################
# OpenBOR executable
#####################
add_executable(${CMAKE_PROJECT_NAME}.elf ${BOR_SRC})
target_include_directories(${CMAKE_PROJECT_NAME}.elf PRIVATE ${BOR_INC})
target_compile_options(${CMAKE_PROJECT_NAME}.elf PRIVATE ${BOR_CFLAGS})
target_link_libraries(${CMAKE_PROJECT_NAME}.elf ${BOR_LDFLAGS})

#####################
# PSP2 (vita) target
#####################
if (BUILD_PSP2)
    vita_create_self(${CMAKE_PROJECT_NAME}.self ${CMAKE_PROJECT_NAME}.elf)
    vita_create_vpk(${CMAKE_PROJECT_NAME}.vpk "OPENBOR01" ${CMAKE_PROJECT_NAME}.self
            VERSION "01.00" NAME "${CMAKE_PROJECT_NAME}"
            FILE ${CMAKE_SOURCE_DIR}/engine/resources/psp2data/sce_sys/icon0.png sce_sys/icon0.png
            FILE ${CMAKE_SOURCE_DIR}/engine/resources/psp2data/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
            FILE ${CMAKE_SOURCE_DIR}/engine/resources/psp2data/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
            FILE ${CMAKE_SOURCE_DIR}/engine/resources/psp2data/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml)
endif (BUILD_PSP2)

#####################
# NX (SWITCH) target
#####################
if (BUILD_NX)
    add_custom_target(${CMAKE_PROJECT_NAME}.nro
            DEPENDS ${CMAKE_PROJECT_NAME}.elf
            COMMAND elf2nro ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.nro)
endif (BUILD_NX)
