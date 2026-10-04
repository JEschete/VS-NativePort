cmake_minimum_required(VERSION 3.28)
include(cmake/WindowsClang.cmake)
project(VagrantStory LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "The maintained build targets Windows; use Visual Studio with Win32.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "The PSX data and GPU tags require a Win32 (x86) configuration.")
endif()
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Select the Visual Studio ClangCL toolset for the decompiled GNU C.")
endif()

include(CTest)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/build/python/Scripts/python.exe" CACHE FILEPATH "Windows build Python")
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(DECOMP_SOURCE "${CMAKE_SOURCE_DIR}/resources/rood-reverseWin")
set(NATIVE_INPUTS "${CMAKE_SOURCE_DIR}/build/native-inputs")
set(NATIVE_DATA "${CMAKE_SOURCE_DIR}/build/native-data")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/windows.py"
    verify-upstream COMMAND_ERROR_IS_FATAL ANY TIMEOUT 120)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/windows.py"
    title-bg-size "${DECOMP_SOURCE}/config/TITLE/TITLE.PRG/splat.yaml"
    OUTPUT_VARIABLE TITLE_BACKGROUND_SIZE OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY TIMEOUT 10)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/native/tools/prepare_native.py"
    "${DECOMP_SOURCE}" "${NATIVE_INPUTS}" COMMAND_ERROR_IS_FATAL ANY TIMEOUT 120)
add_custom_target(NativeInputs
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/windows.py" verify-upstream
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/native/tools/prepare_native.py"
        "${DECOMP_SOURCE}" "${NATIVE_INPUTS}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VERBATIM)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

add_library(vs_host STATIC native/platform/host.c)
target_include_directories(vs_host PUBLIC native/include)
target_link_libraries(vs_host PUBLIC winmm)

function(vs_low_address_target target)
    target_link_options(${target} PRIVATE /BASE:0x10000 /DYNAMICBASE:NO /FIXED /LARGEADDRESSAWARE)
    set_property(TARGET ${target} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

add_executable(test_host native/tests/test_host.c)
target_link_libraries(test_host PRIVATE vs_host)
vs_low_address_target(test_host)

add_executable(test_gte native/tests/test_gte.c native/platform/gte_core.c
    native/platform/libgte.c "${NATIVE_INPUTS}/src/SLUS_010.40/31724.c")
target_include_directories(test_gte PRIVATE native/include)
set_source_files_properties("${NATIVE_INPUTS}/src/SLUS_010.40/31724.c" PROPERTIES
    COMPILE_OPTIONS "/clang:-nostdinc;/clang:-std=gnu89"
    INCLUDE_DIRECTORIES "${NATIVE_INPUTS}/include/psx;${NATIVE_INPUTS}/src/include;${NATIVE_INPUTS}"
    COMPILE_DEFINITIONS "NATIVE_BUILD;_LANGUAGE_C")
vs_low_address_target(test_gte)

add_executable(test_gpu native/tests/test_gpu.c native/platform/libgpu.c
    native/platform/psx_vram.c native/platform/present.c)
target_link_libraries(test_gpu PRIVATE vs_host)
vs_low_address_target(test_gpu)

add_executable(test_inline_gte native/tests/test_inline_gte.c native/platform/inline_gte.c
    native/platform/gte_core.c)
target_include_directories(test_inline_gte PRIVATE native/include)
vs_low_address_target(test_inline_gte)

add_custom_target(WindowsTools
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/windows.py" bootstrap
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_custom_target(Decomp
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/windows.py" decomp
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET Decomp PROPERTY FOLDER "Decompilation")

set(HARVEST_SYMBOLS vs_battle_menuStrings vs_battle_characterWidths vs_battle_rowAnimationSteps
    D_800EB708 D_800EB7B4 D_800EB7D4 D_8002F548 D_8002F54C D_8002F550 D_8002F5A0
    D_8002F600 D_8002F620 D_8002F63C D_8002F66C D_8002F69C D_8002F89C
    g_Sound_StereoPanGainTableQ15 D_8002F99C D_8002F9B4 D_8002F9C0 D_8002F9D4 D_8002F9E0
    D_8002F9F4 D_8002FA00 D_8002FB40 D_8002FB94 D_8002FC3C D_8002FC6C D_8002FCF4
    D_8002FD1C D_8002FF24 D_8003012C D_80030184)
add_custom_command(OUTPUT "${NATIVE_DATA}/assets.c" "${NATIVE_DATA}/disc_index.c"
    "${NATIVE_DATA}/harvested_data.c"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/windows.py" extract
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/native/tools/prepare_data.py"
        "${DECOMP_SOURCE}" "${CMAKE_SOURCE_DIR}/data" "${NATIVE_DATA}/lbas.txt" "${NATIVE_DATA}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/native/tools/harvest_data.py"
        "${NATIVE_DATA}/harvested_data.c" "${NATIVE_DATA}/harvested.syms" ${HARVEST_SYMBOLS}
    DEPENDS tools/windows.py native/tools/prepare_data.py native/tools/harvest_data.py
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" VERBATIM)
add_custom_target(NativeData DEPENDS "${NATIVE_DATA}/assets.c" "${NATIVE_DATA}/disc_index.c"
    "${NATIVE_DATA}/harvested_data.c")

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${NATIVE_INPUTS}/src/*.c")
list(FILTER GAME_SOURCES EXCLUDE REGEX "/(38C1C|4A0A8|COUNTER|INTR|SNMAIN|overlay|LIBPRESS)\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "/MENU/(MENUB\\.PRG/260|MENUF\\.PRG/3B8)\\.c$")
add_library(game_objects OBJECT ${GAME_SOURCES})
add_dependencies(game_objects NativeData NativeInputs)
target_compile_definitions(game_objects PRIVATE NATIVE_BUILD _LANGUAGE_C MENUBGSZ=${TITLE_BACKGROUND_SIZE})
target_compile_options(game_objects PRIVATE /J /w /clang:-std=gnu89 /clang:-nostdinc
    /clang:-fno-builtin /clang:-fno-strict-aliasing /clang:-fwrapv
    /clang:-Wno-error=return-mismatch
    /DMENUBGSZ=${TITLE_BACKGROUND_SIZE}
    "/FI${CMAKE_SOURCE_DIR}/native/include/psx_compat.h")
target_include_directories(game_objects PRIVATE "${NATIVE_INPUTS}/include/psx"
    "${NATIVE_INPUTS}/src/include" "${NATIVE_INPUTS}" "${NATIVE_DATA}" native/include)

set(PLATFORM_SOURCES native/platform/unimpl.c native/platform/scratchpad.c
    native/platform/gte_core.c native/platform/libgte.c native/platform/inline_gte.c native/platform/libgpu.c
    native/platform/libetc.c native/platform/psx_vram.c native/platform/libapi.c
    native/platform/battle_asm.c native/platform/battle_menu.c native/platform/battle_textbox.c
    native/platform/libpad.c native/platform/present.c native/platform/libds.c native/platform/libspu.c
    native/platform/kernel.c native/platform/sound_asm.c native/platform/sound_tables.c
    native/platform/audio_sink.c native/platform/cdxa.c native/platform/mdec.c native/platform/libpress.c
    native/platform/libstream.c native/platform/spu_core.c native/platform/main.c
    "${NATIVE_DATA}/assets.c" "${NATIVE_DATA}/disc_index.c" "${NATIVE_DATA}/harvested_data.c")
add_library(platform_objects OBJECT ${PLATFORM_SOURCES})
add_dependencies(platform_objects NativeData)
target_include_directories(platform_objects PRIVATE native/include native/platform)
target_compile_options(platform_objects PRIVATE /clang:-fno-strict-aliasing /clang:-fwrapv /w)
add_custom_target(NativeStubs
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/native/tools/coff_stubs.py"
        "${CMAKE_BINARY_DIR}" "${DECOMP_SOURCE}/src" "${NATIVE_DATA}/stubs.c"
    BYPRODUCTS "${NATIVE_DATA}/stubs.c"
    DEPENDS game_objects platform_objects native/tools/coff_stubs.py VERBATIM)
add_executable(NativePort "${NATIVE_DATA}/stubs.c"
    $<TARGET_OBJECTS:game_objects> $<TARGET_OBJECTS:platform_objects>)
add_dependencies(NativePort NativeStubs)
target_link_libraries(NativePort PRIVATE vs_host dbghelp)
target_include_directories(NativePort PRIVATE native/include)
set_target_properties(NativePort PROPERTIES OUTPUT_NAME "vagrant-native")
vs_low_address_target(NativePort)

set(SDL2_RUNTIME_DLL "${CMAKE_SOURCE_DIR}/resources/SDL2-2.32.10-win32-x86/SDL2.dll"
    CACHE FILEPATH "Optional Win32 SDL2 runtime DLL")
if(EXISTS "${SDL2_RUNTIME_DLL}")
    add_custom_target(SDL2Runtime
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:NativePort>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SDL2_RUNTIME_DLL}" "$<TARGET_FILE_DIR:NativePort>/SDL2.dll"
        DEPENDS "${SDL2_RUNTIME_DLL}" VERBATIM)
    add_dependencies(NativePort SDL2Runtime)
endif()

if(BUILD_TESTING)
    add_test(NAME WindowsHost COMMAND test_host)
    add_test(NAME GTE COMMAND test_gte "${CMAKE_SOURCE_DIR}/native/tests/gte_golden.txt")
    add_test(NAME GPU COMMAND test_gpu)
    add_test(NAME InlineGTE COMMAND test_inline_gte)
    set_tests_properties(WindowsHost GTE GPU PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 15)
endif()
