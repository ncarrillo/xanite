cmake_minimum_required(VERSION 3.23.1)

project(Xanite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -fPIC")

if(ANDROID)
    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_SYSTEM_VERSION 29)
    set(ANDROID_PLATFORM android-29)
    set(ANDROID_ABI arm64-v8a)
    set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
    set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK})
    set(CMAKE_ANDROID_STL_TYPE c++_shared)

    add_compile_definitions(ARM_NEON=1 ARM64=1)
    add_compile_options(-march=armv8-a)
endif()

find_library(log-lib log)
find_library(android-lib android)

add_library(
    xbox_memory
    SHARED
    Xbox_og/xbox_memory.cpp
)
target_link_libraries(xbox_memory ${log-lib} ${android-lib})

add_library(
    xbox_utils
    SHARED
    Xbox_og/xbox_utils.cpp
)
target_link_libraries(xbox_utils ${log-lib} ${android-lib})

add_library(
    x86_core
    SHARED
    Xbox_og/x86_core.cpp
)
target_link_libraries(x86_core xbox_memory xbox_utils ${log-lib} ${android-lib})

add_library(
    nv2a_renderer
    SHARED
    Xbox_og/nv2a_renderer.cpp
)
target_link_libraries(nv2a_renderer xbox_memory xbox_utils ${log-lib} ${android-lib})

add_library(
    xbox_kernel
    SHARED
    Xbox_og/xbox_kernel.cpp
)
target_link_libraries(xbox_kernel xbox_memory xbox_utils x86_core ${log-lib} ${android-lib})

add_library(
    xbox_emulator
    SHARED
    Xbox_og/xbox_emulator.cpp
)
target_link_libraries(xbox_emulator 
    xbox_memory
    xbox_utils
    xbox_kernel
    x86_core
    nv2a_renderer
    ${log-lib}
    ${android-lib}
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -latomic")
