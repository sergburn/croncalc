#!/usr/bin/env bash
# Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

set -ev

find . -iname "*.gc??" | xargs rm -f

./build.sh clean -DCRON_CALC_WITH_COVERAGE=1 $@

cd .cmake
./cron_calc_test

ls -lR
find . -iname "*.o*" > obj_files
cat obj_files
cat obj_files | xargs gcov -b -c

bash <(curl -s https://codecov.io/bash)
