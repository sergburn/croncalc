#!/usr/bin/env sh

set -e

if [ -d .cmake ]; then
    find .cmake -iname "*.gcda" | xargs rm -f
    find .cmake -iname "*.gcno" | xargs rm -f
fi

./build.sh clean -G Ninja -DCRON_CALC_WITH_COVERAGE=1

gcovr --html --output cov.html -d
