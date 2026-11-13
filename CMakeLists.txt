cmake_minimum_required(VERSION 3.28)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(SIDERA C ASM)
if(NOT CMAKE_C_COMPILER_ID STREQUAL "cc65")
    message(FATAL_ERROR "SIDERA requires cc65; choose cc65/Debug or cc65/Release.")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RP6502_EMU_VERSION 0.43)

add_library(sidera_platform STATIC
    src/platform/rp6502.c src/platform/key.c)
target_include_directories(sidera_platform PUBLIC src/platform)

add_executable(sidera
    # Keep the mainargs override out of an archive: cc65 also supplies a default.
    src/platform/argv.c
    src/main.c src/startup.c src/astro.c src/cities.c src/constellations.c src/cover.c
    src/editor.c src/files.c src/graphics.c src/riseset.c src/timezone.c src/find.c src/target.c src/figure.c src/world_map.c
    src/solar_draw.c src/solar_labels.c src/solar_math.c src/solar_ui.c
    src/overlays/boot.c src/overlays/init_files.c src/overlays/overlay.c)
target_compile_definitions(sidera PRIVATE SIDERA_OVERLAYS)
target_link_libraries(sidera PRIVATE sidera_platform)
target_link_options(sidera PRIVATE
    -C "${CMAKE_CURRENT_SOURCE_DIR}/src/overlays/overlay.cfg"
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
