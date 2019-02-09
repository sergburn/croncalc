#!/usr/bin/env bash
# Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

set -e

find . -iname "*.gcov" | xargs rm -f
find . -iname "*.gcda" | xargs rm -f
find . -iname "*.gcno" | xargs rm -f

./build.sh clean -G Ninja -DCRON_CALC_WITH_COVERAGE=1

cd .cmake
./cron_calc_test

find . -iname "*.obj" | xargs gcov

bash <(curl -s https://codecov.io/bash)
