cmake_minimum_required(VERSION 3.24)
get_filename_component(WORKSPACE "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
include("${WORKSPACE}/cmake/WindowsClang.cmake")
set(CMAKE_C_FLAGS "--target=i686-pc-windows-msvc")
set(CMAKE_CXX_FLAGS "--target=i686-pc-windows-msvc")
project(BattlePositionVerification LANGUAGES C CXX)

enable_testing()
add_executable(test_battle_position test_battle_position.cpp
    "${WORKSPACE}/native/platform/gte_core.c"
    "${WORKSPACE}/native/platform/inline_gte.c")
target_include_directories(test_battle_position PRIVATE "${WORKSPACE}/native/include")
set_source_files_properties(test_battle_position.cpp PROPERTIES COMPILE_OPTIONS
    "/Gy;/clang:-mno-ms-bitfields;/clang:-Wno-macro-redefined")
set_source_files_properties(test_battle_position.cpp PROPERTIES INCLUDE_DIRECTORIES
    "${WORKSPACE}/resources/rood-reverseWin/src/include;${WORKSPACE}/resources/rood-reverseWin/include/psx")
target_link_options(test_battle_position PRIVATE /OPT:REF)
add_test(NAME BattlePosition COMMAND test_battle_position)
set_tests_properties(BattlePosition PROPERTIES TIMEOUT 10)