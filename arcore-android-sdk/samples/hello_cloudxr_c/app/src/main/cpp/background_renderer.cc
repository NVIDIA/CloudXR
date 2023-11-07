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

// This modules handles drawing the passthrough camera image into the OpenGL
// scene.

#include <type_traits>

#include "background_renderer.h"

namespace hello_ar {
namespace {
// Positions of the quad vertices in clip space (X, Y).
const GLfloat kVertices[] = {
    -1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f,
};

const GLfloat kUVs[] = {
    0.0f, 0.0f,
   +1.0f, 0.0f,
    0.0f,+1.0f,
   +1.0f,+1.0f,
};

constexpr char kVertexShaderFilename[] = "shaders/screenquad.vert";
constexpr char kFragmentShaderFilename[] = "shaders/screenquad_ext.frag";
constexpr char kFragmentShaderFilenameScreen[] = "shaders/screenquad.frag";
}  // namespace

void BackgroundRenderer::InitializeGlContent(AAssetManager* asset_manager,
    int width, int height) {
  width_ = width;
  height_ = height;

  shader_program_ = util::CreateProgram(kVertexShaderFilename,
                                        kFragmentShaderFilename, asset_manager);
  if (!shader_program_) {
    CXR_LOGE("Could not create program.");
  }

  shader_program_screen_ = util::CreateProgram(kVertexShaderFilename,
                                               kFragmentShaderFilenameScreen,
                                               asset_manager);

  if (!shader_program_screen_) {
    CXR_LOGE("Could not create program.");
  }

  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id_);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glGenTextures(kQueueLen, texture_ids_);
  glGenFramebuffers(1, &fbo_);

  for (uint32_t idx = 0; idx < kQueueLen; idx++) {
    glBindTexture(GL_TEXTURE_2D, texture_ids_[idx]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }

  uniform_texture_ = glGetUniformLocation(shader_program_, "sTexture");
  attribute_vertices_ = glGetAttribLocation(shader_program_, "a_Position");
  attribute_uvs_ = glGetAttribLocation(shader_program_, "a_TexCoord");
}

void BackgroundRenderer::Draw(const ArSession* session, const ArFrame* frame,
    int offset) {
  static_assert(std::extent<decltype(kVertices)>::value == kNumVertices * 2,
                "Incorrect kVertices length");

  // If display rotation changed (also includes view size change), we need to
  // re-query the uv coordinates for the on-screen portion of the camera image.
  int32_t geometry_changed = 0;
  ArFrame_getDisplayGeometryChanged(session, frame, &geometry_changed);
  if (geometry_changed != 0 || !uvs_initialized_) {
    ArFrame_transformCoordinates2d(
        session, frame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES,
        kNumVertices, kVertices, AR_COORDINATES_2D_TEXTURE_NORMALIZED,
        transformed_uvs_);
    uvs_initialized_ = true;
  }

  int64_t frame_timestamp;
  ArFrame_getTimestamp(session, frame, &frame_timestamp);
  if (frame_timestamp == 0) {
    // Suppress rendering if the camera did not produce the first frame yet.
    // This is to avoid drawing possible leftover data from previous sessions if
    // the texture is reused.
    return;
  }

  const bool render_to_screen = offset >= 0;

  glUseProgram(render_to_screen ? shader_program_screen_ : shader_program_);
  glDepthMask(GL_FALSE);

  if (render_to_screen) {
    // Render to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  } else {
    // Render to internal queue
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, texture_ids_[current_texture_], 0);

    glViewport(0, 0, width_, height_);

    current_texture_ = (current_texture_ + 1)%kQueueLen;
  }

  glUniform1i(uniform_texture_, 1);
  glActiveTexture(GL_TEXTURE1);

  if (render_to_screen) {
    offset++;

    const int idx = current_texture_ < offset ?
        (kQueueLen + (current_texture_ - offset))%kQueueLen :
        (current_texture_ - offset)%kQueueLen;

    glBindTexture(GL_TEXTURE_2D, texture_ids_[idx]);
  } else {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id_);
  }

  glEnableVertexAttribArray(attribute_vertices_);
  glVertexAttribPointer(attribute_vertices_, 2, GL_FLOAT, GL_FALSE, 0,
                        kVertices);

  glEnableVertexAttribArray(attribute_uvs_);
  glVertexAttribPointer(attribute_uvs_, 2, GL_FLOAT, GL_FALSE, 0,
                        offset >= 0 ? kUVs : transformed_uvs_);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glUseProgram(0);
  glDepthMask(GL_TRUE);
  util::CheckGlError("BackgroundRenderer::Draw() error");

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint BackgroundRenderer::GetTextureId() const { return texture_id_; }

}  // namespace hello_ar
