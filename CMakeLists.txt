#
# pdaggerq - A code for bringing strings of creation / annihilation operators to normal order.
# Filename: CMakeLists.txt
# Copyright (C) 2020 A. Eugene DePrince III
#
# Author: A. Eugene DePrince III <adeprince@fsu.edu>
# Maintainer: DePrince group
#
# This file is part of the pdaggerq package.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

cmake_minimum_required(VERSION 3.12)
project(pdaggerq LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# find pybind11 and set variables for finding the correct python version
set(PYBIND11_PYTHON_VERSION 3)
set(PYBIND11_FINDPYTHON ON)
set(Python3_FIND_STRATEGY LOCATION)

include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11
    GIT_TAG        v2.11.1
)

FetchContent_GetProperties(pybind11)
if(NOT pybind11_POPULATED)
    FetchContent_Populate(pybind11)
    add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
endif()

set(PYBIND11_CPP_STANDARD -std=c++17)

# find openmp and set variables (required for pq_graph)
find_package(OpenMP)
if (OpenMP_CXX_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    include_directories(${OpenMP_CXX_INCLUDE_DIRS})
endif()

# we are adding a private module _pdaggerq so we don't have a naming collision
pybind11_add_module(_pdaggerq
        pdaggerq/pq_tensor.cc
        pdaggerq/pq_string.cc
        pdaggerq/pq_utils.cc
        pdaggerq/pq_bernoulli.cc
        pdaggerq/pq_serialize.cc
        pdaggerq/pq_swap_operators.cc
        pdaggerq/pq_add_spin_labels.cc
        pdaggerq/pq_add_label_ranges.cc
        pdaggerq/pq_cumulant_expansion.cc
        pdaggerq/pq_helper.cc

        pq_graph/include/line.hpp
        pq_graph/include/shape.hpp
        pq_graph/src/vertex.cc
        pq_graph/src/linkage.cc
        pq_graph/src/linkage_ops.cc
        pq_graph/include/linkage_set.hpp
        pq_graph/include/scaling_map.hpp
        pq_graph/src/term.cc
        pq_graph/src/substitute.cc
        pq_graph/src/term_perm_helper.cc
        pq_graph/src/equation.cc
        pq_graph/src/pq_graph.cc
        pq_graph/src/consolidate.cc
        pq_graph/src/fusion.cc
        pq_graph/src/graph_printing.cc
        pq_graph/src/vertex_printing.cc
        pq_graph/src/dot_generator.cc
        pq_graph/src/timer.cc

)
