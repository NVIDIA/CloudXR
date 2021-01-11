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

#ifndef C_ARCORE_HELLOE_AR_HELLO_AR_APPLICATION_H_
#define C_ARCORE_HELLOE_AR_HELLO_AR_APPLICATION_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/asset_manager.h>
#include <jni.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "arcore_c_api.h"
#include "background_renderer.h"
#include "glm.h"
#include "plane_renderer.h"
#include "util.h"

namespace hello_ar {

// HelloArApplication handles all application logics.
class HelloArApplication {
 public:
  // Constructor and deconstructor.
  //HelloArApplication() = default;
  HelloArApplication(AAssetManager* asset_manager);
  ~HelloArApplication();

  bool Init();
  void HandleLaunchOptions(std::string &cmdline);
  void SetArgs(const std::string &args);
  std::string GetServerIp();

  // OnPause is called on the UI thread from the Activity's onPause method.
  void OnPause();

  // OnResume is called on the UI thread from the Activity's onResume method.
  void OnResume(void* env, void* context, void* activity);

  // OnSurfaceCreated is called on the OpenGL thread when GLSurfaceView
  // is created.
  void OnSurfaceCreated();

  // OnDisplayGeometryChanged is called on the OpenGL thread when the
  // render surface size or display rotation changes.
  //
  // @param display_rotation: current display rotation.
  // @param width: width of the changed surface view.
  // @param height: height of the changed surface view.
  void OnDisplayGeometryChanged(int display_rotation, int width, int height);

  // OnDrawFrame is called on the OpenGL thread to render the next frame.
  void OnDrawFrame();

  // OnTouched is called on the OpenGL thread after the user touches the screen.
  // @param x: x position on the screen (pixels).
  // @param y: y position on the screen (pixels).
  // @param longPress: a long press occured.
  void OnTouched(float x, float y, bool longPress);

  // Returns true if any planes have been detected.  Used for hiding the
  // "searching for planes" snackbar.
  bool HasDetectedPlanes() const {
    return plane_count_ > 0 || using_image_anchors_ || base_frame_calibrated_;
  }

 private:
  void UpdateImageAnchors();
  void UpdateCloudAnchor();

  ArSession* ar_session_ = nullptr;
  ArFrame* ar_frame_ = nullptr;
  ArCameraIntrinsics* ar_camera_intrinsics_ = nullptr;
  ArAnchor* anchor_ = nullptr;
  ArAnchor* cloud_anchor_ = nullptr;

  bool install_requested_ = false;
  int display_width_ = 1;
  int display_height_ = 1;
  int display_rotation_ = 0;
  int cam_image_width_ = 1920;
  int cam_image_height_ = 1080;

  bool using_image_anchors_ = false;
  std::unordered_map<int32_t, std::pair<ArAugmentedImage*, ArAnchor*>>
      augmented_image_map;

  bool using_dynamic_base_frame_ = true;
  bool base_frame_calibrated_ = false;
  glm::mat4 base_frame_;

  AAssetManager* const asset_manager_;

  BackgroundRenderer background_renderer_;
  PlaneRenderer plane_renderer_;

  int32_t plane_count_ = 0;

  // CloudXR client interface class
  class CloudXRClient;
  std::unique_ptr<CloudXRClient> cloudxr_client_;
};
}  // namespace hello_ar

#endif  // C_ARCORE_HELLOE_AR_HELLO_AR_APPLICATION_H_
