#!/usr/bin/env bash

set -e

if [ "$1" == "clean" ]; then
    echo "Clean build!"
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
