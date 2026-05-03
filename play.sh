#!/bin/bash

if [ ! -f "build/Makefile" ]; then
    cmake -B build
fi

cmake --build build -j4 && ./build/cardbox
