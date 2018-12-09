#!/usr/bin/env bash

if [ ! -d .cmake ]; then
    mkdir .cmake
fi

pushd .cmake
cmake ..

cmake --build .

popd
