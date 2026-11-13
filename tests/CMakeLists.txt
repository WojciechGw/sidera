cmake_minimum_required(VERSION 3.20)
project(sidera_checks C)
if(NOT MSVC)
    link_libraries(m)
endif()
enable_testing()
set(CMAKE_CTEST_ARGUMENTS --output-on-failure)
add_executable(sidera_riseset_host riseset_host.c ../src/astro.c ../src/timezone.c)
target_include_directories(sidera_riseset_host PRIVATE host ../src ../src/platform)
target_compile_definitions(sidera_riseset_host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sidera_riseset_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME sidera_world_map COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/check_world_map.py")
add_test(NAME sidera_riseset COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_riseset.py"
    "$<TARGET_FILE:sidera_riseset_host>")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_host.py"
    "${CMAKE_CURRENT_BINARY_DIR}/graphics_host.c" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "../src/graphics.c" "../src/main.c" "../src/editor.c" "prepare_host.py")
set(SOLAR_SOURCES ../src/solar_ui.c ../src/solar_math.c ../src/solar_draw.c ../src/solar_labels.c ../src/find.c ../src/target.c ../src/figure.c ../src/world_map.c)
add_executable(sidera_host ${SOLAR_SOURCES} host_test.c ../src/startup.c ../src/overlays/boot.c ../src/astro.c ../src/files.c ../src/cities.c ../src/timezone.c ../src/riseset.c ../src/constellations.c "${CMAKE_CURRENT_BINARY_DIR}/graphics_host.c")
target_include_directories(sidera_host PRIVATE host ../src ../src/platform "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(sidera_host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sidera_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME sidera_host COMMAND sidera_host "${CMAKE_CURRENT_SOURCE_DIR}/../src/rom/sidera.dat"
    "${CMAKE_CURRENT_BINARY_DIR}/sidera.xram")
set_tests_properties(sidera_host PROPERTIES FIXTURES_SETUP sidera_snapshots)
add_test(NAME sidera_views COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/reference_views.py"
    "${CMAKE_CURRENT_BINARY_DIR}/sidera.xram" "${CMAKE_CURRENT_BINARY_DIR}/views")
set_tests_properties(sidera_views PROPERTIES FIXTURES_REQUIRED sidera_snapshots)
set_tests_properties(sidera_views PROPERTIES FIXTURES_SETUP sidera_rendered_views)
add_test(NAME sidera_reference_pixels COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/check_reference_views.py" "${CMAKE_CURRENT_BINARY_DIR}/views")
set_tests_properties(sidera_reference_pixels PROPERTIES FIXTURES_REQUIRED sidera_rendered_views)
add_executable(sidera_app_host ${SOLAR_SOURCES} app_host.c ../src/startup.c ../src/overlays/boot.c ../src/astro.c ../src/files.c ../src/cities.c ../src/timezone.c ../src/riseset.c ../src/constellations.c ../src/cover.c
    "${CMAKE_CURRENT_BINARY_DIR}/graphics_host.c")
target_include_directories(sidera_app_host PRIVATE host ../src ../src/platform)
target_compile_definitions(sidera_app_host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sidera_app_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME sidera_app_host COMMAND sidera_app_host "${CMAKE_CURRENT_SOURCE_DIR}/../src/rom"
    "${CMAKE_CURRENT_BINARY_DIR}/sidera-app-test.cfg")
add_executable(sidera_overlay_host ${SOLAR_SOURCES} app_host.c ../src/startup.c ../src/overlays/boot.c ../src/astro.c ../src/files.c ../src/cities.c ../src/timezone.c ../src/riseset.c ../src/constellations.c ../src/cover.c
    ../src/overlays/overlay.c ../src/overlays/init_files.c "${CMAKE_CURRENT_BINARY_DIR}/graphics_host.c")
target_include_directories(sidera_overlay_host PRIVATE host ../src ../src/platform)
target_compile_definitions(sidera_overlay_host PRIVATE SIDERA_OVERLAYS SIDERA_OVERLAY_HOST _CRT_SECURE_NO_WARNINGS)
target_compile_options(sidera_overlay_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
set(SIDERA_PAYLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/cc65/debug/sidera" CACHE PATH "Built sidera overlay assets")
add_test(NAME sidera_overlay_loop COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_overlay_loop.py"
    "$<TARGET_FILE:sidera_overlay_host>" "${SIDERA_PAYLOAD_DIR}")
set_tests_properties(sidera_overlay_loop PROPERTIES TIMEOUT 60)
add_executable(sidera_cover_host cover_host.c)
target_include_directories(sidera_cover_host PRIVATE host ../src ../src/platform)
target_compile_definitions(sidera_cover_host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sidera_cover_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME sidera_cover_host COMMAND sidera_cover_host "${CMAKE_CURRENT_SOURCE_DIR}/../src/rom/sidera.dat"
    "${CMAKE_CURRENT_BINARY_DIR}/sidera-cover.xram")
set_tests_properties(sidera_cover_host PROPERTIES TIMEOUT 10)
add_test(NAME sidera_cover_asset COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_cover.py")
find_program(CL65 cl65 REQUIRED)
find_program(SIM65 sim65 REQUIRED)
add_test(NAME sidera_platform_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_platform_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/platform_sim")
set_tests_properties(sidera_platform_6502 PROPERTIES TIMEOUT 60)
add_test(NAME sidera_loader_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_overlay_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/overlay_sim")
add_test(NAME sidera_solar_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_solar_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/solar_sim")
add_test(NAME sidera_settings_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_settings_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/settings_sim")
add_test(NAME sidera_riseset_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_riseset_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/riseset_sim")
add_test(NAME sidera_fast_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_fast_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/fast_sim")
add_test(NAME sidera_projection_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_projection_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/projection_sim")
add_test(NAME sidera_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/sim")
add_test(NAME sidera_stars_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_stars.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/stars_sim")
add_test(NAME sidera_planets_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_planets.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/planets_sim")
add_executable(sidera_constellation_test constellation_test.c ../src/astro.c ../src/figure.c)
target_include_directories(sidera_constellation_test PRIVATE host ../src ../src/platform)
target_compile_options(sidera_constellation_test PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME sidera_constellations COMMAND sidera_constellation_test "${CMAKE_CURRENT_SOURCE_DIR}/../src/rom/sidera.dat")
set_tests_properties(sidera_host sidera_app_host sidera_constellations PROPERTIES TIMEOUT 30)
add_test(NAME sidera_catalogue COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_catalogue.py")
add_test(NAME sidera_find_catalogue COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_find_catalogue.py")
add_executable(sidera_timezone_host timezone_host.c ../src/timezone.c ../src/astro.c)
target_include_directories(sidera_timezone_host PRIVATE host ../src ../src/platform)
target_compile_definitions(sidera_timezone_host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sidera_timezone_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME sidera_timezones COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_timezones.py"
    "$<TARGET_FILE:sidera_timezone_host>")
add_test(NAME sidera_timezone_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_timezone_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/timezone_sim")

add_executable(sidera_labels_host solar_labels_test.c ../src/solar_labels.c ../src/solar_math.c ../src/astro.c)
target_include_directories(sidera_labels_host PRIVATE ../src)
target_compile_options(sidera_labels_host PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME sidera_labels COMMAND sidera_labels_host)
set_tests_properties(sidera_labels PROPERTIES TIMEOUT 60)
add_test(NAME sidera_labels_6502 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_labels_6502.py"
    "${CL65}" "${SIM65}" "${CMAKE_CURRENT_BINARY_DIR}/labels_sim")
set_tests_properties(sidera_labels_6502 PROPERTIES TIMEOUT 90)
set_tests_properties(sidera_timezones PROPERTIES TIMEOUT 180)
