if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR "The maintained host build requires Windows.")
endif()

find_program(VSWHERE_EXECUTABLE vswhere.exe
    HINTS "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer"
    REQUIRED)
execute_process(
    COMMAND "${VSWHERE_EXECUTABLE}" -latest -products * -version "[17,18)"
        -find "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    OUTPUT_VARIABLE VS_CLANG_EXECUTABLE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
    TIMEOUT 10)
if(NOT EXISTS "${VS_CLANG_EXECUTABLE}")
    message(FATAL_ERROR "Install the Visual Studio 2022 C++ Clang compiler component.")
endif()
get_filename_component(VS_CLANG_DIRECTORY "${VS_CLANG_EXECUTABLE}" DIRECTORY)
file(TO_CMAKE_PATH "${VS_CLANG_DIRECTORY}" VS_CLANG_DIRECTORY)
set(CMAKE_VS_GLOBALS
    "CLToolExe=clang-cl.exe;CLToolPath=${VS_CLANG_DIRECTORY}/"
    CACHE STRING "Installed Clang compiler used by the Visual Studio C++ toolset" FORCE)
set(ENV{CL} "--target=i686-pc-windows-msvc")
