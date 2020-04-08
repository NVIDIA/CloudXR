/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef C_ARCORE_HELLO_AR_BACKGROUND_RENDERER_H_
#define C_ARCORE_HELLO_AR_BACKGROUND_RENDERER_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/asset_manager.h>
#include <cstdlib>

#include "arcore_c_api.h"
#include "util.h"

namespace hello_ar {

// This class renders the passthrough camera image into the OpenGL frame.
class BackgroundRenderer {
 public:
  static constexpr int kQueueLen = 16;

  BackgroundRenderer() = default;
  ~BackgroundRenderer() = default;

  // Sets up OpenGL state.  Must be called on the OpenGL thread and before any
  // other methods below.
  void InitializeGlContent(AAssetManager* asset_manager, int width, int height);

  // Draws the background image.  This methods must be called for every ArFrame
  // returned by ArSession_update() to catch display geometry change events.
  //
  // Maintains internal look-back circular array of camera images of kQueueLen length.
  // frame_offset is an offset from the current pointer in camera images array.
  // When image_offset < 0 draws image to the internal array and advances the
  // array pointer.
  void Draw(const ArSession* session, const ArFrame* frame, int frame_offset=-1);

  // Returns the generated texture name for the GL_TEXTURE_EXTERNAL_OES target.
  GLuint GetTextureId() const;

 private:
  static constexpr int kNumVertices = 4;

  GLuint shader_program_;
  GLuint shader_program_screen_;

  GLuint texture_id_;
  GLuint fbo_;

  GLuint texture_ids_[kQueueLen];
  int current_texture_ = 0;

  GLuint attribute_vertices_;
  GLuint attribute_uvs_;
  GLuint uniform_texture_;

  int width_ = 1920;
  int height_ = 1080;

  float transformed_uvs_[kNumVertices * 2];
  bool uvs_initialized_ = false;
};
}  // namespace hello_ar
#endif  // C_ARCORE_HELLO_AR_BACKGROUND_RENDERER_H_
