cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(VirtualVitaRAStick C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# Kernel plugins do not use the normal Vita application crt0/main entrypoint.
# Keep -nostdlib on the compiler/link command, matching established VitaSDK
# kernel-plugin projects, and retain relocations for vita-elf-create.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -std=gnu99 -nostdlib -Wl,-q")

add_executable(${PROJECT_NAME}
  src/main.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE __VITA_KERNEL__)

target_link_libraries(${PROJECT_NAME}
  SceSysmemForDriver_stub
  taihenForKernel_stub
  k
  gcc
)

vita_create_self(${PROJECT_NAME}.skprx ${PROJECT_NAME}
  CONFIG exports.yml
  UNSAFE
)
