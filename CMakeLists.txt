cmake_minimum_required(VERSION 3.11)
project(lost_soul_of_yor C)

# main.c stays at the repository root for the PSPSDK build.mak workflow.
add_executable(lost_soul_of_yor main.c)
target_link_libraries(lost_soul_of_yor PRIVATE pspgu pspgum pspdisplay pspge pspctrl m)

# Some PSP CMake toolchains provide this packaging helper. The supported,
# complete EBOOT.PBP build path is Makefile plus PSPSDK lib/build.mak.
if(COMMAND create_pbp_file)
  create_pbp_file(TARGET lost_soul_of_yor
    TITLE "Lost Soul of Yor"
    VERSION 01.00
    DESCRIPTION "A dark fantasy PSP action RPG prototype")
else()
  message(STATUS "No PSP CMake packaging helper found; use make to create EBOOT.PBP.")
endif()
