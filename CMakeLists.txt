cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(SIDERA C ASM)
if(NOT CMAKE_C_COMPILER_ID STREQUAL "cc65")
    message(FATAL_ERROR "SIDERA requires cc65; choose cc65/Debug or cc65/Release.")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RP6502_EMU_VERSION 0.43)

add_library(sidera_common STATIC
    src/common/rpc_common/errtext.c src/common/rpc_common/ftype.c
    src/common/rpc_common/hand.c src/common/rpc_common/key.c
    src/common/rpc_common/pathx.c src/common/rpc_common/scr_dlg.c
    src/common/rpc_common/scr_ed.c src/common/rpc_common/scr_keys.c
    src/common/rpc_common/scr_menu.c src/common/rpc_common/scr_msg.c
    src/common/rpc_common/scr_out.c src/common/rpc_common/scr_term.c
    src/common/rpc_common/scr_text.c src/common/rpc_common/textcut.c
    src/common/rpc_common/util.c src/common/rpc_common/xram.c
    src/common/rpc_common/xr_gets.c src/common/rpc_common/xr_move.c)
target_include_directories(sidera_common PUBLIC src/common/rpc_common src/platform)
add_library(sidera_platform STATIC
    src/platform/rpmt_bare.c src/platform/rpmt_os_file.c src/platform/rpmt_os_dir.c)
target_include_directories(sidera_platform PUBLIC src/platform)
target_compile_definitions(sidera_platform PRIVATE RPMT_BARE)

add_executable(sidera
    src/main.c src/astro.c src/cities.c src/constellations.c src/cover.c
    src/editor.c src/files.c src/graphics.c src/riseset.c src/timezone.c
    src/solar_draw.c src/solar_labels.c src/solar_math.c src/solar_ui.c
    src/bare/boot.c src/bare/init_files.c src/bare/overlay.c)
target_compile_definitions(sidera PRIVATE RPMT_BARE SIDERA_OVERLAYS)
target_link_libraries(sidera PRIVATE sidera_common sidera_platform)
target_link_options(sidera PRIVATE
    -C "${CMAKE_CURRENT_SOURCE_DIR}/src/bare/overlay.cfg"
    -Ln "${CMAKE_CURRENT_BINARY_DIR}/sidera/sidera.lbl")
include(cmake/SideraOverlays.cmake)
sidera_overlays(sidera)

# WEB is deliberately inactive. Its SDK/template files remain available.
# No rp6502_web() call: no WEB download, package, or launch target.
add_custom_target(sidera_assets
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/scripts/regenerate.py"
        --output "${CMAKE_CURRENT_BINARY_DIR}/generated"
    VERBATIM)
include(cmake/SideraTests.cmake)
