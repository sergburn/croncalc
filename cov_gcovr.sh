#!/usr/bin/env sh

set -ev

if [ -d .cmake ]; then
    find .cmake -iname "*.gcda" | xargs rm -f
    find .cmake -iname "*.gcno" | xargs rm -f
fi

./build.sh clean -DCRON_CALC_WITH_COVERAGE=1 $@
.cmake/cron_calc_test
gcovr --html-details --output cov.html -d
