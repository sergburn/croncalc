# Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

project(CronCalc)
cmake_minimum_required(VERSION 3.6)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CRON_CALC_COVERAGE --coverage)
    set(CRON_CALC_COVERAGE_LIB gcov)
else()
    set(CRON_CALC_COVERAGE "")
    set(CRON_CALC_COVERAGE_LIB "")
endif()

add_library(cron_calc_c STATIC src/cron_calc.c)
target_compile_options(cron_calc_c PRIVATE -std=c99 -Wall -Werror -pedantic ${CRON_CALC_COVERAGE})

add_library(cron_calc_cpp STATIC src/cron_calc.cpp)
target_compile_options(cron_calc_cpp PRIVATE -std=c++98 -Wall -Werror -pedantic ${CRON_CALC_COVERAGE})

add_executable(cron_calc_test test/cron_calc_test.cpp)
target_compile_options(cron_calc_test PRIVATE -std=c++11 ${CRON_CALC_COVERAGE})

target_link_libraries(cron_calc_test PRIVATE cron_calc_cpp cron_calc_c ${CRON_CALC_COVERAGE_LIB})

target_include_directories(cron_calc_test
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)
