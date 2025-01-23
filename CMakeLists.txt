cmake_minimum_required(VERSION 3.22)
project(untitled5 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")

add_executable(untitled5 main.c)
