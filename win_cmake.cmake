# Windows cache init for CMake (-C win_cmake.cmake)
# Keeps configuration focused on MSVC builds.

set(CMAKE_C_COMPILER "cl" CACHE STRING "Use MSVC C compiler" FORCE)
set(CMAKE_CXX_COMPILER "cl" CACHE STRING "Use MSVC C++ compiler" FORCE)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard" FORCE)
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Disable compiler extensions" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)
