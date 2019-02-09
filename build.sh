#!/usr/bin/env bash
# Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

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
