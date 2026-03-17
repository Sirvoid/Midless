#!/usr/bin/env bash

if [ ! -f "stb_perlin.h" ]; then
    wget https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_perlin.h
fi

gcc -o midless.exe *.c \
    -I. \
    -IC:/raylib/raylib/src \
    -IC:/raylib/raylib/src/external \
    -L C:/raylib/raylib/src \
    -static -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -lws2_32

./midless
