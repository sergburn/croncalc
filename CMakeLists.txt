# Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

project(CronCalc)
cmake_minimum_required(VERSION 3.6)

if(${CRON_CALC_WITH_COVERAGE})
    message("Coverage intstrumentation enabled.")
    set(CRON_CALC_COVERAGE --coverage)
else()
    set(CRON_CALC_COVERAGE "")
endif()

add_library(cron_calc_c STATIC src/cron_calc.c)
target_compile_options(cron_calc_c PRIVATE -std=c99 -Wall -Werror -pedantic ${CRON_CALC_COVERAGE})

add_library(cron_calc_cpp STATIC src/cron_calc.cpp)
target_compile_options(cron_calc_cpp PRIVATE -std=c++98 -Wall -Werror -pedantic ${CRON_CALC_COVERAGE})

add_executable(cron_calc_test test/cron_calc_test.cpp)
target_compile_options(cron_calc_test PRIVATE -std=c++11 ${CRON_CALC_COVERAGE})
if(${CRON_CALC_TEST_VERBOSE})
    target_compile_definitions(cron_calc_test PRIVATE CRON_CALC_TEST_VERBOSE)
endif()

target_link_libraries(cron_calc_test PRIVATE cron_calc_cpp cron_calc_c ${CRON_CALC_COVERAGE})

target_include_directories(cron_calc_test
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)
