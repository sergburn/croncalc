#!/usr/bin/env sh
cd .cmake
find . -iname "*.gcda" | xargs rm
./cron_calc_test
cd ..
gcovr --html-details --output cov.html -d
