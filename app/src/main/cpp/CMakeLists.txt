cmake_minimum_required(VERSION 3.22.1)
project(fe7native LANGUAGES CXX)

add_library(fe7native SHARED
    fe7native.cpp
)

target_compile_features(fe7native PRIVATE cxx_std_17)
target_compile_options(fe7native PRIVATE -Wall -Wextra)
