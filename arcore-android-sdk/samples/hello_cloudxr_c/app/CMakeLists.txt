# Copyright (C) 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##

#Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
#
#Permission is hereby granted, free of charge, to any person obtaining a
#copy of this software and associated documentation files (the "Software"),
#to deal in the Software without restriction, including without limitation
#the rights to use, copy, modify, merge, publish, distribute, sublicense,
#and/or sell copies of the Software, and to permit persons to whom the
#Software is furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
#THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#DEALINGS IN THE SOFTWARE.

# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Import the ARCore library.
add_library(arcore SHARED IMPORTED)
set_target_properties(arcore PROPERTIES IMPORTED_LOCATION
                      ${ARCORE_LIBPATH}/${ANDROID_ABI}/libarcore_sdk_c.so
                      INTERFACE_INCLUDE_DIRECTORIES ${ARCORE_INCLUDE}
)

# Import the glm header file from the NDK.
add_library( glm INTERFACE )
set_target_properties( glm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GLM_INCLUDE})

# Import CloudXR
add_library(cloudxr-lib SHARED IMPORTED)
set_target_properties(cloudxr-lib PROPERTIES IMPORTED_LOCATION
                      ${CLOUDXR_LIBPATH}/${ANDROID_ABI}/libCloudXRClient.so
)
include_directories(${CLOUDXR_INCLUDE})

# Include shared headers
include_directories(${C_SHARED_INCLUDE})

# This is the main app library.
add_library(hello_cloudxr_native SHARED
           src/main/cpp/background_renderer.cc
           src/main/cpp/hello_ar_application.cc
           src/main/cpp/jni_interface.cc
           src/main/cpp/plane_renderer.cc
           src/main/cpp/util.cc
           src/main/cpp/blitter.cc)

target_include_directories(hello_cloudxr_native PRIVATE
           src/main/cpp)
target_link_libraries(hello_cloudxr_native
                      cloudxr-lib
                      android
                      log
                      GLESv2
                      EGL
                      glm
                      arcore)
