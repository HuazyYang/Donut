#
# Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.


file(GLOB donut_core_tests src/core/test_*.cpp)

foreach(test_src ${donut_core_tests})

    get_filename_component(test_name "${test_src}" NAME_WE)
    #message(STATUS "Added test ${test_name}")

    add_executable("${test_name}" "${test_src}")
    target_link_libraries("${test_name}" donut_core donut_tests_utils)

    add_dependencies(donut_all_tests "${test_name}")

    add_test("${test_name}" "${test_name}")

    set_property(TARGET "${test_name}" PROPERTY FOLDER "Donut/donut_tests/donut_core_tests")

endforeach()


if(1)

find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_auto_ptr GTest::gtest GTest::gtest_main)

add_library(VLD SHARED IMPORTED)
set_target_properties(
    VLD
    PROPERTIES
    IMPORTED_LOCATION "$ENV{VLD_INSTALL_DIR}/bin/Win64/vld_x64.dll"
    IMPORTED_IMPLIB "$ENV{VLD_INSTALL_DIR}/lib/Win64/vld.lib"
    INTERFACE_INCLUDE_DIRECTORIES "$ENV{VLD_INSTALL_DIR}/include"
)

target_link_libraries(test_auto_ptr VLD)
endif()
