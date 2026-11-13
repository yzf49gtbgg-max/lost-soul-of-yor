cmake_minimum_required(VERSION 3.11)
project(lost_soul_of_yor C)

add_executable(lost_soul_of_yor src/main.c)
target_link_libraries(lost_soul_of_yor PRIVATE pspgu pspgum pspdisplay pspge pspctrl m)
create_pbp_file(TARGET lost_soul_of_yor
  TITLE "Lost Soul of Yor"
  VERSION 01.00
  DESCRIPTION "A dark fantasy PSP action RPG prototype")
