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

# VitaSDK deliberately places -Wl,-q in CMAKE_C_FLAGS because the compiler
# driver is also used for the final link. Keep that convention here; it
# preserves relocation sections required by vita-elf-create.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -std=gnu99 -Wl,-q")

add_executable(${PROJECT_NAME}
  src/main.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE __VITA_KERNEL__)

# Kernel plugins do not use the normal crt0/main startup.
set_target_properties(${PROJECT_NAME} PROPERTIES
  LINK_FLAGS "-nostdlib"
)

target_link_libraries(${PROJECT_NAME}
  SceSysmemForDriver_stub
  taihenForKernel_stub
  k
  gcc
)

# This bootstrap does not import any Vita API functions yet, so permit an
# empty import table. Once real SceTouch/SceCtrl/taiHEN imports are used this
# can be revisited.
set(VITA_ELF_CREATE_FLAGS "${VITA_ELF_CREATE_FLAGS} -n")

vita_create_self(${PROJECT_NAME}.skprx ${PROJECT_NAME}
  CONFIG exports.yml
  UNSAFE
)
