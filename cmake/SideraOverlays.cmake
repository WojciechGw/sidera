# SIDERA extensions live outside tools/, which the SDK updater replaces.
function(sidera_overlays target)
    set(work "${CMAKE_CURRENT_BINARY_DIR}/${target}")
    set(pack "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack_overlays.py")
    set(check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_overlays.py")
    set(outputs "${work}/sidera.bin")
    foreach(name init cities edit cover solar)
        list(APPEND outputs "${work}/sidera-${name}.ovl"
            "${work}/sidera.linked.bin.${name}")
        rp6502_byproduct(${target} "sidera-${name}.ovl" "sidera-${name}.ovl")
    endforeach()
    rp6502_byproduct(${target} RAM(0x0200) sidera.bin)
    file(GLOB assets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/rom/*")
    foreach(asset IN LISTS assets)
        get_filename_component(name "${asset}" NAME)
        if(name STREQUAL "sidera-manual-en.txt")
            set(name manual)
        endif()
        rp6502_asset(${target} "${name}" "${asset}")
    endforeach()
    rp6502_asset(${target} help src/help.txt)
    rp6502_asset(${target} sidera-gpl.txt LICENSE)
    rp6502_asset(${target} sidera-notice.txt LICENSE-NOTICE.txt)
    rp6502_asset(${target} sidera-sdk-license.txt LICENSES/RP6502-SDK.txt)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/overlays/overlay.cfg" "${pack}" "${check}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -B "${pack}" "${work}"
        BYPRODUCTS ${outputs} "${work}/sidera.lbl"
        VERBATIM)
    rp6502_executable(${target} RESET 0x0200)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -B "${check}"
            "${RP6502_TOOLS_DIR}/rp6502.py" "${work}"
            "$<TARGET_PROPERTY:${target},RP6502_ROM>"
        VERBATIM)
endfunction()
