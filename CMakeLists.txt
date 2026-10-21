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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -std=gnu99")

# This bootstrap is a kernel module, not a normal application with crt0/main.
# Keep the standard startup/runtime out of the link, but explicitly preserve
# relocation information for vita-elf-create.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib -Wl,-q")

# The bootstrap currently imports no Vita APIs yet, so allow an empty import
# table. We can remove -n once SceTouch/SceCtrl/taiHEN imports are actually used.
set(VITA_ELF_CREATE_FLAGS "${VITA_ELF_CREATE_FLAGS} -n")

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
