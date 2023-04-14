cmake_minimum_required(VERSION 3.3)



SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wwritable-strings")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwritable-strings")

if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
        set(OS_DEPENDENDED_SRC
                aarch64/ffi_arm64.c
                aarch64/sysv_arm64.S
                )

elseif (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
        set(OS_DEPENDENDED_SRC
                arm/ffi_armv7.c
                arm/sysv_armv7.S
                )

endif()

include_directories(
        ./
        aarch64
        arm
        platform_include
)
add_library(
        libffi

        STATIC

        closures.c
        debug.c
        dlmalloc.c
        java_raw_api.c
        prep_cif.c
        raw_api.c
        ffi_cxx.cc
        types.c
        ${OS_DEPENDENDED_SRC}
        )

target_link_libraries(
        libffi


        ${log-lib}
)

