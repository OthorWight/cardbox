#!/bin/bash

if [ ! -f "build/Makefile" ]; then
    cmake -B build
fi

cmake --build build -j4

if [ -z "$CI" ]; then
    ./build/cardbox
fi
