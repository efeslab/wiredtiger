include(CheckCCompilerFlag)
include(cmake/rcpc_test.cmake)

set(WT_ARCH "aarch64" CACHE STRING "")
set(WT_OS "linux" CACHE STRING "")
set(WT_POSIX ON CACHE BOOL "")

# Linux requires '_GNU_SOURCE' to be defined for access to GNU/Linux extension functions
# e.g. Access to O_DIRECT on Linux. Append this macro to our compiler flags for Linux-based
# builds.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE" CACHE STRING "" FORCE)

# ARMv8-A is the 64-bit ARM architecture, turn on the optional CRC.
# If the compilation check in rcpc_test passes also turn on the RCpc instructions.
if(HAVE_RCPC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8.2-a+rcpc+crc" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.2-a+rcpc+crc" CACHE STRING "" FORCE)
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+crc" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+crc" CACHE STRING "" FORCE)
endif()

check_c_compiler_flag("-moutline-atomics" has_moutline_atomics)
# moutline-atomics preserves backwards compatibility with Arm v8.0 systems but also supports
# using Arm v8.1 atomics. The latter can massively improve performance on larger Arm systems.
# The flag was back ported to gcc8, 9 and is the default in gcc10+. See if the compiler supports
# the flag.
if(has_moutline_atomics)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -moutline-atomics" CACHE STRING "" FORCE)
endif()
unset(has_moutline_atomics CACHE)

# Enable ARM Neon SIMD instrinsics when available.
CHECK_INCLUDE_FILE("arm_neon.h" have_arm_neon)
if(have_arm_neon)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_ARM_NEON_INTRIN_H" CACHE STRING "" FORCE)
endif()
unset(has_arm_neon CACHE)
