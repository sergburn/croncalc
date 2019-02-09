#!/usr/bin/env bash

set -e

if [ -d .cmake ]; then
    find .cmake -iname "*.gcda" | xargs rm -f
    find .cmake -iname "*.gcno" | xargs rm -f
fi

./build.sh clean -G Ninja -DCRON_CALC_WITH_COVERAGE=1

lcov --capture --directory . --output-file coverage.info

genhtml coverage.info --output-directory cov
