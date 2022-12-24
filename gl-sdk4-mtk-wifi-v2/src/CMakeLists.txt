cmake_minimum_required(VERSION 3.0)

project(iwpriv C)

add_definitions(-D_GNU_SOURCE)
add_compile_options(-O -Wall -Werror --std=gnu99)

add_library(iwpriv MODULE iwpriv.c)
set_target_properties(iwpriv PROPERTIES OUTPUT_NAME iwpriv PREFIX "")

install(
    TARGETS iwpriv
    DESTINATION lib/lua
)
