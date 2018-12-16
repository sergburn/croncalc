#!/usr/bin/env bash

set -ev

if [ "$1" == "clean" ]; then
    rm -rf .cmake
    shift
fi

if [ ! -d .cmake ]; then
    mkdir .cmake
fi

cd .cmake
cmake .. $@
cmake --build .

./cron_calc_test
