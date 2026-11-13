# Native host tests use their own compiler, never the cc65 toolchain.
include(ExternalProject)
if(CMAKE_HOST_WIN32)
    set(SIDERA_HOST_GENERATOR "Visual Studio 17 2022" CACHE STRING "Native test CMake generator")
    set(host_platform CMAKE_GENERATOR_PLATFORM x64)
else()
    set(SIDERA_HOST_GENERATOR "Ninja" CACHE STRING "Native test CMake generator")
endif()
ExternalProject_Add(sidera_host_tests
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/host-native"
    CMAKE_GENERATOR "${SIDERA_HOST_GENERATOR}"
    ${host_platform}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DSIDERA_PAYLOAD_DIR=${CMAKE_CURRENT_BINARY_DIR}/sidera
        -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
    INSTALL_COMMAND ""
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config ${CMAKE_BUILD_TYPE}
    BUILD_ALWAYS TRUE
    DEPENDS sidera)
add_test(NAME sidera_host_suite
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_CURRENT_BINARY_DIR}/host-native"
        -C "${CMAKE_BUILD_TYPE}" --output-on-failure)
set_tests_properties(sidera_host_suite PROPERTIES TIMEOUT 300)
add_test(NAME sidera_rom
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_overlays.py"
        "${RP6502_TOOLS_DIR}/rp6502.py" "${CMAKE_CURRENT_BINARY_DIR}/sidera"
        "$<TARGET_PROPERTY:sidera,RP6502_ROM>")
rp6502_test(NAME sidera_emu COMMAND tests/emu_smoke.py
    --memseed 123 "$<TARGET_PROPERTY:sidera,RP6502_ROM>"
    -- /config SAVE:/sidera.cfg /date "2026-11-12 20:00:00")
set_tests_properties(sidera_emu PROPERTIES TIMEOUT 90)
rp6502_test(NAME sidera_input COMMAND tests/measure_input_latency.py
    --memseed 123 --phi2 8000 "$<TARGET_PROPERTY:sidera,RP6502_ROM>"
    -- /config SAVE:/input.cfg /date "2026-11-12 20:00:00"
    /grid 3 /paths on /constellations 3 /planets on /view "180,24")
set_tests_properties(sidera_input PROPERTIES TIMEOUT 90)
set_property(TEST sidera_input APPEND PROPERTY ENVIRONMENT "SIDERA_CHECK_LATENCY=1")
add_test(NAME sidera_generated_assets
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/scripts/regenerate.py"
        --output "${CMAKE_CURRENT_BINARY_DIR}/generated" --check)
set_tests_properties(sidera_generated_assets PROPERTIES TIMEOUT 900)
