# Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

project(CronCalc)
cmake_minimum_required(VERSION 3.6)

add_library(cron_calc_c STATIC src/cron_calc.c)
target_compile_options(cron_calc_c PRIVATE -std=c99 -Wall -Werror -pedantic)

if(${CRON_CALC_WITH_COVERAGE})
    message("Coverage intstrumentation enabled.")
    target_compile_options(cron_calc_c PUBLIC --coverage)
    target_compile_definitions(cron_calc_c PUBLIC CRON_CALC_WITH_COVERAGE)
endif()

if (UNIX)
    target_compile_definitions(cron_calc_c PUBLIC _POSIX_C_SOURCE=200809L)
endif()

add_library(cron_calc_cpp STATIC src/cron_calc.cpp)
target_compile_options(cron_calc_cpp PRIVATE -std=c++98 -Wall -Werror -pedantic)
target_link_libraries(cron_calc_cpp PUBLIC cron_calc_c)

add_executable(cron_calc_test test/cron_calc_test.cpp)
target_compile_options(cron_calc_test PRIVATE -std=c++11)
if(${CRON_CALC_TEST_VERBOSE})
    target_compile_definitions(cron_calc_test PRIVATE CRON_CALC_TEST_VERBOSE)
endif()

target_link_libraries(cron_calc_test PRIVATE cron_calc_cpp)
if(${CRON_CALC_WITH_COVERAGE})
    target_link_libraries(cron_calc_test PRIVATE --coverage)
endif()

target_include_directories(cron_calc_test
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)
