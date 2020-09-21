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
#include "blitter.h"

#include <stdio.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "util.h"

static const char* kHeader = R"(#version 310 es
)";

static const char* kQuadVS = R"(
#extension GL_EXT_shader_io_blocks : enable

precision highp float;

layout(location = 0) out vec2 vsUV0;

out gl_PerVertex {
  vec4 gl_Position;
};

const vec2 positions[4] = vec2[4](
  vec2(-1.0, -1.0),
  vec2( 1.0, -1.0),
  vec2(-1.0,  1.0),
  vec2( 1.0,  1.0)
);

void main() {
  gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
  vsUV0 = positions[gl_VertexID]/2.0 + vec2(0.5, 0.5);
}
)";

static const char* kQuadFS = R"(
precision highp float;

layout(location = 0) uniform sampler2D uTexture0;
layout(location = 1) uniform sampler2D uTexture1;
layout(location = 2) uniform vec4 uColorCorrection;
layout(location = 0) in vec2 vsUV0;
layout(location = 0) out vec4 fsColor;

void main() {
  fsColor = texture(uTexture0, vsUV0);
  fsColor.a = texture(uTexture1, vsUV0).x;

  // Apply color correction
  const float kMiddleGrayGamma = 0.466;
  fsColor.rgb *= uColorCorrection.rgb*uColorCorrection.a/kMiddleGrayGamma;
}
)";

static constexpr uint32_t kTexture0Uniform = 0;
static constexpr uint32_t kTexture1Uniform = 1;
static constexpr uint32_t kColorCorrectionUniform = 2;

bool Blitter::InitBlitProgram() {
  if (blit_program_ != 0)
    return true;

  GLuint VS = glCreateShader(GL_VERTEX_SHADER);
  {
    const GLint sizes[] = { static_cast<GLint>(strlen(kHeader)),
                            static_cast<GLint>(strlen(kQuadVS)) };
    const char* lines[] = { kHeader, kQuadVS };
    glShaderSource(VS, 2, lines, sizes);
    glCompileShader(VS);
  }
  GLuint FS = glCreateShader(GL_FRAGMENT_SHADER);
  {
    const GLint sizes[] = { static_cast<GLint>(strlen(kHeader)),
                            static_cast<GLint>(strlen(kQuadFS)) };
    const char* lines[] = { kHeader, kQuadFS };
    glShaderSource(FS, 2, lines, sizes);
    glCompileShader(FS);
  }

  blit_program_ = glCreateProgram();

  glAttachShader(blit_program_, VS);
  glAttachShader(blit_program_, FS);

  glLinkProgram(blit_program_);

  GLint status = GL_TRUE;
  glGetProgramiv(blit_program_, GL_LINK_STATUS, &status);

  if (GL_FALSE == status)
  {
    char str[1024];
    glGetProgramInfoLog(blit_program_, sizeof(str), NULL, str);
    LOGE("Program linking failed:\n%s", str);

    glDeleteProgram(blit_program_);
    blit_program_ = 0;
  }

  glDeleteShader(VS);
  glDeleteShader(FS);

  return blit_program_ != 0;
}

bool Blitter::BlitTexture(uint32_t dst, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, uint32_t src,
    uint32_t src_alpha, const float color_correction[4]) {

  if (!InitBlitProgram()) {
    return false;
  }

  if (!blit_fbo_) {
    glGenFramebuffers(1, &blit_fbo_);
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, src);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);

  glUseProgram(blit_program_);

  if (dst) {
    glBindFramebuffer(GL_FRAMEBUFFER, blit_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, dst, 0);
  }

  if (width > 0 && height > 0) {
    glViewport(x, y, width, height);
  }

  glUniform1i(kTexture0Uniform, 0);

  if (src_alpha) {
    // Assuming premultiplied alpha
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, src_alpha);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUniform1i(kTexture1Uniform, 1);
  }

  if (!color_correction) {
    static float kNoColorCorrection[4] = { 1.f, 1.f, 1.f, 0.466f };
    color_correction = kNoColorCorrection;
  }

  glUniform4fv(kColorCorrectionUniform, 1, color_correction);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glUseProgram(0);

  if (dst) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  if (src_alpha) {
    glDisable(GL_BLEND);
  }

  return true;
}
