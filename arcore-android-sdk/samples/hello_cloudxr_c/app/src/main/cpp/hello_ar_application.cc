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

#include "hello_ar_application.h"

#include <android/asset_manager.h>
#include <array>
#include <mutex>
#include <EGL/egl.h>

#include "plane_renderer.h"
#include "util.h"

#include "CloudXR.h"
#include "blitter.h"
#include "CloudXRLaunchOptions.h"

namespace hello_ar {
namespace {
const glm::vec3 kWhite = {255, 255, 255};
}  // namespace

class ARLaunchOptions : public CloudXR::LaunchOptions {
public:
    bool using_env_lighting_;
    float res_factor_;

    ARLaunchOptions() :
      LaunchOptions(),
      using_env_lighting_(true), // default ON
      // default to 0.75 reduced size, as many devices can't handle full throughput.
      // 0.75 chosen as WAR value for steamvr buffer-odd-size bug, works on galaxytab s6 + pixel 2
      res_factor_(0.75f)
    { }

protected:
    // we override for extra cmdline options...
    virtual void HandleArg(std::string &tok)
    {
      if (tok == "-e" || tok == "-envlighting") { // env lighting flag
        GetNextToken(tok);
        if (tok=="1" || tok=="on") {
          using_env_lighting_ = true;
        }
        else if (tok=="0" || tok=="off") {
          using_env_lighting_ = false;
        }
        // else leave as whatever default is...
      }
      else if (tok == "-r" || tok == "-resfactor") { // resfactor override
        GetNextToken(tok);
        float factor = std::stof(tok);
        if (factor >= 0.5f && factor <= 1.0f)
          res_factor_ = factor;
      }
      else
        LaunchOptions::HandleArg(tok);
    }
};

class HelloArApplication::CloudXRClient : public CloudXR::Client {
 public:
  ~CloudXRClient() override {
    Teardown();
  }

  void TriggerHaptic(const CloudXR::HapticFeedback*) override {}
  void RenderAudio(const CloudXR::AudioFrame*) override {}

  void GetTrackingState(CloudXR::VRTrackingState* state) override {
    *state = {};

    state->hmd.pose.bPoseIsValid = true;
    state->hmd.pose.bDeviceIsConnected = true;
    state->hmd.pose.eTrackingResult = CloudXR::TrackingResult_Running_OK;

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      const int idx = current_idx_ == 0 ?
          kQueueLen - 1 : (current_idx_ - 1)%kQueueLen;
      state->hmd.pose.mDeviceToAbsoluteTracking = hmd_matrix_[idx];
    }
  }

  CloudXR::HMDParams GetHMDParams() {
    hmd_params_.width = stream_width_;
    hmd_params_.height = stream_height_;
    hmd_params_.maxRes = stream_max_res_;
    hmd_params_.fps = static_cast<float>(fps_);
    hmd_params_.ipd = 0.064f;
    hmd_params_.predOffset = -0.02f;
    hmd_params_.audio = 0;
    hmd_params_.rightAlpha = 1;

    return hmd_params_;
  }

  void Connect() {
    if (cloudxr_receiver_)
      return;

    LOGI("Connecting to CloudXR at %s...", launch_options_.mServerIP.c_str());

    CloudXR::GraphicsContext context{CloudXR::GraphicsContext_GLES};
    context.egl.display = eglGetCurrentDisplay();
    context.egl.context = eglGetCurrentContext();

    auto hmd_params = GetHMDParams();

    if (CloudXR::CreateReceiver(launch_options_.mServerIP.c_str(), launch_options_.mLogLevel,
        &hmd_params, this, &context, &cloudxr_receiver_) == CloudXR::Error_Success) {
      // AR shouldn't have an arena, should it?  Maybe something large?
      //LOGI("Setting default 1m radius arena boundary.", result);
      //CloudXR::SetArenaBoundary(Receiver, 10.f, 0, 0);
    }

    if (!IsRunning())
      Teardown();
  }

  void Teardown() {
    if (cloudxr_receiver_) {
      LOGI("Tearing down CloudXR...");
      CloudXR::DestroyReceiver(cloudxr_receiver_);
    }

    cloudxr_receiver_ = nullptr;
  }

  bool IsRunning() const {
    return cloudxr_receiver_ && CloudXR::IsRunning(cloudxr_receiver_);
  }

  void SetHMDMatrix(const glm::mat4& hmd_mat) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto& hmd_matrix = hmd_matrix_[current_idx_];

    hmd_matrix.m[0][0] = hmd_mat[0][0];
    hmd_matrix.m[0][1] = hmd_mat[1][0];
    hmd_matrix.m[0][2] = hmd_mat[2][0];
    hmd_matrix.m[0][3] = hmd_mat[3][0];
    hmd_matrix.m[1][0] = hmd_mat[0][1];
    hmd_matrix.m[1][1] = hmd_mat[1][1];
    hmd_matrix.m[1][2] = hmd_mat[2][1];
    hmd_matrix.m[1][3] = hmd_mat[3][1];
    hmd_matrix.m[2][0] = hmd_mat[0][2];
    hmd_matrix.m[2][1] = hmd_mat[1][2];
    hmd_matrix.m[2][2] = hmd_mat[2][2];
    hmd_matrix.m[2][3] = hmd_mat[3][2];

    current_idx_ = (current_idx_ + 1)%kQueueLen;
  }

  void SetProjectionMatrix(const glm::mat4& projection) {
    if (fabsf(projection[2][0]) > 0.0001f) {
      // Non-symmetric projection
      const float oneOver00 = 1.f/projection[0][0];
      const float l = -(1.f - projection[2][0])*oneOver00;
      const float r = 2.f*oneOver00 + l;

      const float oneOver11 = 1.f/projection[1][1];
      const float b = -(1.f - projection[2][1])*oneOver11;
      const float t = 2.f*oneOver11 + b;

      hmd_params_.proj[0][0] = l;
      hmd_params_.proj[0][1] = r;
      hmd_params_.proj[0][2] = -t;
      hmd_params_.proj[0][3] = -b;
    } else {
      // Symmetric projection
      hmd_params_.proj[0][0] = -1.f/projection[0][0];
      hmd_params_.proj[0][1] = -hmd_params_.proj[0][0];
      hmd_params_.proj[0][2] = -1.f/projection[1][1];
      hmd_params_.proj[0][3] = -hmd_params_.proj[0][2];
    }

    hmd_params_.proj[1][0] = hmd_params_.proj[0][0];
    hmd_params_.proj[1][1] = hmd_params_.proj[0][1];

    // Disable right eye rendering
    hmd_params_.proj[1][2] = 0;
    hmd_params_.proj[1][3] = 0;

    LOGI("Proj: %f %f %f %f", hmd_params_.proj[0][0], hmd_params_.proj[0][1],
        hmd_params_.proj[0][2], hmd_params_.proj[0][3]);
  }

  void SetFps(int fps) {
    fps_ = fps;
  }

  int DetermineOffset() const {
    for (int offset = 0; offset < kQueueLen; offset++) {
      const int idx = current_idx_ < offset ?
          kQueueLen + (current_idx_ - offset)%kQueueLen :
          (current_idx_ - offset)%kQueueLen;

      const auto& hmd_matrix = hmd_matrix_[idx];

      if (fabsf(hmd_matrix.m[0][0] - frame_.hmdMatrix.m[0][0]) < 0.0001f &&
          fabsf(hmd_matrix.m[0][1] - frame_.hmdMatrix.m[0][1]) < 0.0001f &&
          fabsf(hmd_matrix.m[0][2] - frame_.hmdMatrix.m[0][2]) < 0.0001f &&
          fabsf(hmd_matrix.m[0][3] - frame_.hmdMatrix.m[0][3]) < 0.0001f &&
          fabsf(hmd_matrix.m[1][0] - frame_.hmdMatrix.m[1][0]) < 0.0001f &&
          fabsf(hmd_matrix.m[1][1] - frame_.hmdMatrix.m[1][1]) < 0.0001f &&
          fabsf(hmd_matrix.m[1][2] - frame_.hmdMatrix.m[1][2]) < 0.0001f &&
          fabsf(hmd_matrix.m[1][3] - frame_.hmdMatrix.m[1][3]) < 0.0001f &&
          fabsf(hmd_matrix.m[2][0] - frame_.hmdMatrix.m[2][0]) < 0.0001f &&
          fabsf(hmd_matrix.m[2][1] - frame_.hmdMatrix.m[2][1]) < 0.0001f &&
          fabsf(hmd_matrix.m[2][2] - frame_.hmdMatrix.m[2][2]) < 0.0001f &&
          fabsf(hmd_matrix.m[2][3] - frame_.hmdMatrix.m[2][3]) < 0.0001f) {
        return offset;
      }
    }

    return 0;
  }

  bool Latch() {
    if (latched_) {
      return true;
    }

    if (!IsRunning()) {
      return false;
    }

    // Fetch the frame
    const uint32_t timeout_ms = 150;
    const bool have_frame = CloudXR::LatchFrame(cloudxr_receiver_,
        &frame_, timeout_ms) == CloudXR::Error_Success;

    if (!have_frame) {
      LOGI("CloudXR frame is not available!");
      return false;
    }

    latched_ = true;

    return true;
  }

  void Release() {
    if (!latched_) {
      return;
    }

    CloudXR::ReleaseFrame(cloudxr_receiver_, &frame_);
    latched_ = false;
  }

  void Render(const float color_correction[4]) {
    if (!IsRunning() || !latched_) {
      return;
    }

    blitter_.BlitTexture(0, 0, 0, 0, 0,
        frame_.eyeTex[0], frame_.eyeTex[1], color_correction);
  }

  void UpdateLightProps(const float primaryDirection[3], const float primaryIntensity[3],
      const float ambient_spherical_harmonics[27]) {
    CloudXR::LightProperties lightProperties;

    for (uint32_t n = 0; n < 3; n++) {
      lightProperties.primaryLightColor.v[n] = primaryIntensity[n];
      lightProperties.primaryLightDirection.v[n] = primaryDirection[n];
    }

    for (uint32_t n = 0; n < CloudXR::MAX_AMBIENT_LIGHT_SH*3; n++) {
      lightProperties.ambientLightSh[n/3].v[n%3] = ambient_spherical_harmonics[n];
    }

    CloudXR::SendLightProperties(cloudxr_receiver_, &lightProperties);
  }


  bool Init() {
    return true;
  }

  bool HandleLaunchOptions(std::string &cmdline) {
    // first, try to read "command line in a text file"
    launch_options_.ParseFile("/sdcard/CloudXRLaunchOptions.txt");
    // next, process actual 'commandline' args -- overrides any prior values
    launch_options_.ParseString(cmdline);

    // we log error here if no server (if have no 'input UI', we have no other source)
    if (launch_options_.mServerIP.empty())
      LOGE("No server IP specified yet to connect to.");

    return true;
  }

  void SetServerAddr(std::string &ip) {
    launch_options_.mServerIP = ip; // note we do no validation here!
  }

  std::string GetServerAddr() {
    return launch_options_.mServerIP;
  }

  bool GetUseEnvLighting() {
    return launch_options_.using_env_lighting_;
  }

  // this is used to tell the client what the display/surface resolution is.
  // here, we can apply a factor to reduce what we tell the server our desired
  // video resolution should be.
  void SetStreamRes(uint32_t w, uint32_t h) {
      if (w>h) { // we want width to be smaller dimension, i.e. 'portrait' oriented.  inline swap.
          uint32_t temp = w;
          w = h;
          h = temp;
      }
      // apply the res factor to width and height, and make sure they are even for stream res.
      stream_width_ = ((uint32_t)round((float)w * launch_options_.res_factor_)) & ~1;
      stream_height_ = ((uint32_t)round((float)h * launch_options_.res_factor_)) & ~1;
      // set max res to be stream height (larger dimension), but clamp to 2048.
      stream_max_res_ = (stream_height_ > 2048) ? 2048 : stream_height_;
      LOGI("SetStreamRes: Display res passed = %dx%d", w, h);
      LOGI("SetStreamRes: Stream res set = %dx%d [max %d]", stream_width_, stream_height_, stream_max_res_);
  }

private:
  static constexpr int kQueueLen = BackgroundRenderer::kQueueLen;

  CloudXR::Receiver* cloudxr_receiver_ = nullptr;

  ARLaunchOptions launch_options_;

  uint32_t stream_width_ = 720;
  uint32_t stream_height_ = 1440;
  uint32_t stream_max_res_ = 1440;

  CloudXR::VideoFrame frame_ = {};
  bool latched_ = false;

  std::mutex state_mutex_;
  CloudXR::HmdMatrix34 hmd_matrix_[kQueueLen] = {};
  CloudXR::HMDParams hmd_params_ = {};
  int current_idx_ = 0;

  Blitter blitter_;

  int fps_ = 60;
};


HelloArApplication::HelloArApplication(AAssetManager* asset_manager)
    : asset_manager_(asset_manager) {
  cloudxr_client_ = std::make_unique<HelloArApplication::CloudXRClient>();
}

HelloArApplication::~HelloArApplication() {
  if (ar_session_ != nullptr) {
    if (ar_camera_intrinsics_ != nullptr) {
      ArCameraIntrinsics_destroy(ar_camera_intrinsics_);
    }

    ArSession_destroy(ar_session_);
    ArFrame_destroy(ar_frame_);
  }
}

// use for any deeper, failure-possible init of app, or cxr client.
bool HelloArApplication::Init() {
  return cloudxr_client_->Init();
}

// pass server address direct to client.
void HelloArApplication::HandleLaunchOptions(std::string &cmdline) {
  cloudxr_client_->HandleLaunchOptions(cmdline);
}

// pass server address direct to client.
void HelloArApplication::SetServerIp(std::string &ip) {
  cloudxr_client_->SetServerAddr(ip);
}

// pass server address direct to client.
std::string HelloArApplication::GetServerIp() {
  return cloudxr_client_->GetServerAddr();
}


void HelloArApplication::OnPause() {
  LOGI("OnPause()");
  if (ar_session_ != nullptr) {
    ArSession_pause(ar_session_);
  }

  cloudxr_client_->Teardown();
}

void HelloArApplication::OnResume(void* env, void* context, void* activity) {
  LOGI("OnResume()");

  if (ar_session_ == nullptr) {
    ArInstallStatus install_status;
    // If install was not yet requested, that means that we are resuming the
    // activity first time because of explicit user interaction (such as
    // launching the application)
    bool user_requested_install = !install_requested_;

    // === ATTENTION!  ATTENTION!  ATTENTION! ===
    // This method can and will fail in user-facing situations.  Your
    // application must handle these cases at least somewhat gracefully.  See
    // HelloAR Java sample code for reasonable behavior.
    CHECK(ArCoreApk_requestInstall(env, activity, user_requested_install,
                                   &install_status) == AR_SUCCESS);

    switch (install_status) {
      case AR_INSTALL_STATUS_INSTALLED:
        break;
      case AR_INSTALL_STATUS_INSTALL_REQUESTED:
        install_requested_ = true;
        return;
    }

    // === ATTENTION!  ATTENTION!  ATTENTION! ===
    // This method can and will fail in user-facing situations.  Your
    // application must handle these cases at least somewhat gracefully.  See
    // HelloAR Java sample code for reasonable behavior.
    CHECK(ArSession_create(env, context, &ar_session_) == AR_SUCCESS);
    CHECK(ar_session_);

    ArFrame_create(ar_session_, &ar_frame_);
    CHECK(ar_frame_);

    ArSession_setDisplayGeometry(ar_session_, display_rotation_, display_width_, display_height_);

    // Retrieve supported camera configs.
    ArCameraConfigList* all_camera_configs = nullptr;
    int32_t num_configs = 0;
    ArCameraConfigList_create(ar_session_, &all_camera_configs);
    // Create filter first to get both 30 and 60 fps.
    ArCameraConfigFilter* camera_config_filter = nullptr;
    ArCameraConfigFilter_create(ar_session_, &camera_config_filter);
    ArCameraConfigFilter_setTargetFps(
        ar_session_, camera_config_filter,
        AR_CAMERA_CONFIG_TARGET_FPS_60);
    ArSession_getSupportedCameraConfigsWithFilter(
        ar_session_, camera_config_filter, all_camera_configs);
    ArCameraConfigList_getSize(ar_session_, all_camera_configs, &num_configs);

    if (num_configs < 1) {
      LOGI("No 60Hz camera available!");
      cloudxr_client_->SetFps(30);
    } else {
      ArCameraConfig* camera_config;
      ArCameraConfig_create(ar_session_, &camera_config);
      ArCameraConfigList_getItem(ar_session_, all_camera_configs, 0,
                                 camera_config);

      ArSession_setCameraConfig(ar_session_, camera_config);
      cloudxr_client_->SetFps(60);
    }

    ArCameraConfigList_destroy(all_camera_configs);

    if (cloudxr_client_->GetUseEnvLighting()) {
      ArConfig* config = nullptr;
      ArConfig_create(ar_session_, &config);
      ArSession_getConfig(ar_session_, config);
      ArConfig_setLightEstimationMode(ar_session_, config,
          AR_LIGHT_ESTIMATION_MODE_ENVIRONMENTAL_HDR);
      ArSession_configure(ar_session_, config);
      ArConfig_destroy(config);
    }
  }

  ArCameraIntrinsics_create(ar_session_, &ar_camera_intrinsics_);

  const ArStatus status = ArSession_resume(ar_session_);
  CHECK(status == AR_SUCCESS);

  ArCamera* ar_camera;
  ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);

  ArCamera_getTextureIntrinsics(ar_session_, ar_camera, ar_camera_intrinsics_);
  ArCameraIntrinsics_getImageDimensions(ar_session_, ar_camera_intrinsics_,
                                        &cam_image_width_, &cam_image_height_);

  LOGI("Camera res: %dx%d", cam_image_width_, cam_image_height_);
}

void HelloArApplication::OnSurfaceCreated() {
  LOGI("OnSurfaceCreated()");

  background_renderer_.InitializeGlContent(asset_manager_, cam_image_width_, cam_image_height_);
  plane_renderer_.InitializeGlContent(asset_manager_);
}

void HelloArApplication::OnDisplayGeometryChanged(int display_rotation,
                                                  int width, int height) {
  LOGI("OnSurfaceChanged(%d, %d)", width, height);
  glViewport(0, 0, width, height);
  display_rotation_ = display_rotation;
  display_width_ = width;
  display_height_ = height;
  if (ar_session_ != nullptr) {
    ArSession_setDisplayGeometry(ar_session_, display_rotation, width, height);
  }
  cloudxr_client_->SetStreamRes(display_width_, display_height_); // TODO TBD should we pre-rotate?
}

void HelloArApplication::OnDrawFrame() {
  // Render the scene.
  glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (ar_session_ == nullptr) return;

  const GLuint camera_texture = background_renderer_.GetTextureId();

  ArSession_setCameraTextureName(ar_session_, camera_texture);

  // Update session to get current frame and render camera background.
  if (ArSession_update(ar_session_, ar_frame_) != AR_SUCCESS) {
    LOGE("HelloArApplication::OnDrawFrame ArSession_update error");
  }

  ArCamera* ar_camera;
  ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);

  glm::mat4 view_mat;
  glm::mat4 projection_mat;
  ArCamera_getViewMatrix(ar_session_, ar_camera, glm::value_ptr(view_mat));
  ArCamera_getProjectionMatrix(ar_session_, ar_camera,
                               /*near=*/0.1f, /*far=*/100.f,
                               glm::value_ptr(projection_mat));

  ArTrackingState camera_tracking_state;
  ArCamera_getTrackingState(ar_session_, ar_camera, &camera_tracking_state);
  ArCamera_release(ar_camera);

  // Draw to camera queue
  background_renderer_.Draw(ar_session_, ar_frame_);

  glViewport(0, 0, display_width_, display_height_);

  if (!cloudxr_client_->IsRunning() || !base_frame_calibrated_) {
    // Draw camera image to the screen
    background_renderer_.Draw(ar_session_, ar_frame_, 0);
  }

  // If the camera isn't tracking don't bother rendering other objects.
  if (camera_tracking_state != AR_TRACKING_STATE_TRACKING) {
    return;
  }

  // We need to (re)calibrate but CloudXR client is running - continue
  // pulling the frames. There'll be a lag otherwise.
  if (!base_frame_calibrated_ && cloudxr_client_->IsRunning()) {
    if (cloudxr_client_->Latch())
      cloudxr_client_->Release();
  }

  if (base_frame_calibrated_) {
    // Try fetch base frame
    if (using_dynamic_base_frame_ && anchor_) {
      ArTrackingState tracking_state = AR_TRACKING_STATE_STOPPED;
      ArAnchor_getTrackingState(ar_session_, anchor_,
                                &tracking_state);
      if (tracking_state == AR_TRACKING_STATE_TRACKING) {
        glm::mat4 anchor_pose_mat(1.0f);

        util::GetTransformMatrixFromAnchor(*anchor_, ar_session_,
                                           &anchor_pose_mat);

        base_frame_ = glm::inverse(anchor_pose_mat);
      }
    }

    if (!cloudxr_client_->IsRunning()) {
      cloudxr_client_->SetProjectionMatrix(projection_mat);
      cloudxr_client_->Connect();
    }

    const bool have_frame = cloudxr_client_->Latch();
    const int pose_offset = have_frame ? cloudxr_client_->DetermineOffset() : 0;

    // Render cached camera frame to the screen
    glViewport(0, 0, display_width_, display_height_);
    background_renderer_.Draw(ar_session_, ar_frame_, pose_offset);

    // Setup HMD matrix with our base frame
    const glm::mat4 cloudxr_hmd_mat = base_frame_*glm::inverse(view_mat);
    cloudxr_client_->SetHMDMatrix(cloudxr_hmd_mat);

    // Set light intensity to default. Intensity value ranges from 0.0f to 1.0f.
    // The first three components are color scaling factors.
    // The last one is the average pixel intensity in gamma space.
    float color_correction[4] = {1.f, 1.f, 1.f, 0.466f};
    {
      // Get light estimation
      ArLightEstimate* ar_light_estimate;
      ArLightEstimateState ar_light_estimate_state;
      ArLightEstimate_create(ar_session_, &ar_light_estimate);

      ArFrame_getLightEstimate(ar_session_, ar_frame_, ar_light_estimate);
      ArLightEstimate_getState(ar_session_, ar_light_estimate,
                               &ar_light_estimate_state);

      if (ar_light_estimate_state == AR_LIGHT_ESTIMATE_STATE_VALID) {
        if (cloudxr_client_->GetUseEnvLighting()) {
          float direction[3];
          ArLightEstimate_getEnvironmentalHdrMainLightDirection(ar_session_,
              ar_light_estimate, direction);

          float intensity[3];
          ArLightEstimate_getEnvironmentalHdrMainLightIntensity(ar_session_,
              ar_light_estimate, intensity);

          float ambient_spherical_harmonics[27];
          ArLightEstimate_getEnvironmentalHdrAmbientSphericalHarmonics(ar_session_,
              ar_light_estimate, ambient_spherical_harmonics);

          cloudxr_client_->UpdateLightProps(direction, intensity, ambient_spherical_harmonics);

        } else {
          ArLightEstimate_getColorCorrection(ar_session_, ar_light_estimate,
                                             color_correction);
        }
      }

      ArLightEstimate_destroy(ar_light_estimate);
    }

    if (have_frame) {
      // Composite CloudXR frame to the screen
      glViewport(0, 0, display_width_, display_height_);
      cloudxr_client_->Render(color_correction);
      cloudxr_client_->Release();
    }
  }

  // Calibrate base frame only when neccessary
  if (base_frame_calibrated_)
    return;

  // Try fetch zero basis
  if (anchor_) {
    ArTrackingState tracking_state = AR_TRACKING_STATE_STOPPED;
    ArAnchor_getTrackingState(ar_session_, anchor_,
                              &tracking_state);

    if (tracking_state == AR_TRACKING_STATE_TRACKING) {
      glm::mat4 anchor_pose_mat(1.0f);

      util::GetTransformMatrixFromAnchor(*anchor_, ar_session_,
                                         &anchor_pose_mat);

      base_frame_ = glm::inverse(anchor_pose_mat);
      base_frame_calibrated_ = true;
    }
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Update and render planes.
  ArTrackableList* plane_list = nullptr;
  ArTrackableList_create(ar_session_, &plane_list);
  CHECK(plane_list != nullptr);

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArSession_getAllTrackables(ar_session_, plane_tracked_type, plane_list);

  int32_t plane_list_size = 0;
  ArTrackableList_getSize(ar_session_, plane_list, &plane_list_size);
  plane_count_ = plane_list_size;

  for (int i = 0; i < plane_list_size; ++i) {
    ArTrackable* ar_trackable = nullptr;
    ArTrackableList_acquireItem(ar_session_, plane_list, i, &ar_trackable);
    ArPlane* ar_plane = ArAsPlane(ar_trackable);
    ArTrackingState out_tracking_state;
    ArTrackable_getTrackingState(ar_session_, ar_trackable,
                                 &out_tracking_state);

    ArPlane* subsume_plane;
    ArPlane_acquireSubsumedBy(ar_session_, ar_plane, &subsume_plane);
    if (subsume_plane != nullptr) {
      ArTrackable_release(ArAsTrackable(subsume_plane));
      continue;
    }

    if (ArTrackingState::AR_TRACKING_STATE_TRACKING != out_tracking_state) {
      continue;
    }

    ArTrackingState plane_tracking_state;
    ArTrackable_getTrackingState(ar_session_, ArAsTrackable(ar_plane),
                                 &plane_tracking_state);
    if (plane_tracking_state == AR_TRACKING_STATE_TRACKING) {
      plane_renderer_.Draw(projection_mat, view_mat, *ar_session_, *ar_plane,
                           kWhite);
      ArTrackable_release(ar_trackable);
    }
  }

  ArTrackableList_destroy(plane_list);
  plane_list = nullptr;
}

void HelloArApplication::OnTouched(float x, float y, bool longPress) {
  // Do not do anything if base frame is calibrated and user
  // does not want to reset it
  if (base_frame_calibrated_ && !longPress)
    return;

  // Reset calibration on a long press
  if (longPress) {
    if (anchor_) {
      ArAnchor_release(anchor_);
      anchor_ = nullptr;
    }

    base_frame_calibrated_ = false;
    return;
  }

  if (ar_frame_ != nullptr && ar_session_ != nullptr) {
    ArHitResultList* hit_result_list = nullptr;
    ArHitResultList_create(ar_session_, &hit_result_list);
    CHECK(hit_result_list);
    ArFrame_hitTest(ar_session_, ar_frame_, x, y, hit_result_list);

    int32_t hit_result_list_size = 0;
    ArHitResultList_getSize(ar_session_, hit_result_list,
                            &hit_result_list_size);

    // The hitTest method sorts the resulting list by distance from the camera,
    // increasing.  The first hit result will usually be the most relevant when
    // responding to user input.

    ArHitResult* ar_hit_result = nullptr;
    ArTrackableType trackable_type = AR_TRACKABLE_NOT_VALID;
    for (int32_t i = 0; i < hit_result_list_size; ++i) {
      ArHitResult* ar_hit = nullptr;
      ArHitResult_create(ar_session_, &ar_hit);
      ArHitResultList_getItem(ar_session_, hit_result_list, i, ar_hit);

      if (ar_hit == nullptr) {
        LOGE("HelloArApplication::OnTouched ArHitResultList_getItem error");
        return;
      }

      ArTrackable* ar_trackable = nullptr;
      ArHitResult_acquireTrackable(ar_session_, ar_hit, &ar_trackable);
      ArTrackableType ar_trackable_type = AR_TRACKABLE_NOT_VALID;
      ArTrackable_getType(ar_session_, ar_trackable, &ar_trackable_type);
      // Creates an anchor if a plane or an oriented point was hit.
      if (AR_TRACKABLE_PLANE == ar_trackable_type) {
        ArPose* hit_pose = nullptr;
        ArPose_create(ar_session_, nullptr, &hit_pose);
        ArHitResult_getHitPose(ar_session_, ar_hit, hit_pose);
        int32_t in_polygon = 0;
        ArPlane* ar_plane = ArAsPlane(ar_trackable);
        ArPlane_isPoseInPolygon(ar_session_, ar_plane, hit_pose, &in_polygon);

        // Use hit pose and camera pose to check if hittest is from the
        // back of the plane, if it is, no need to create the anchor.
        ArPose* camera_pose = nullptr;
        ArPose_create(ar_session_, nullptr, &camera_pose);
        ArCamera* ar_camera;
        ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);
        ArCamera_getPose(ar_session_, ar_camera, camera_pose);
        ArCamera_release(ar_camera);
        float normal_distance_to_plane = util::CalculateDistanceToPlane(
            *ar_session_, *hit_pose, *camera_pose);

        ArPose_destroy(hit_pose);
        ArPose_destroy(camera_pose);

        if (!in_polygon || normal_distance_to_plane < 0) {
          continue;
        }

        ar_hit_result = ar_hit;
        trackable_type = ar_trackable_type;
        break;
      } else if (AR_TRACKABLE_POINT == ar_trackable_type) {
        ArPoint* ar_point = ArAsPoint(ar_trackable);
        ArPointOrientationMode mode;
        ArPoint_getOrientationMode(ar_session_, ar_point, &mode);
        if (AR_POINT_ORIENTATION_ESTIMATED_SURFACE_NORMAL == mode) {
          ar_hit_result = ar_hit;
          trackable_type = ar_trackable_type;
          break;
        }
      }
    }

    if (ar_hit_result) {
      // Note that the application is responsible for releasing the anchor
      // pointer after using it. Call ArAnchor_release(anchor) to release.
      ArAnchor* anchor = nullptr;
      if (ArHitResult_acquireNewAnchor(ar_session_, ar_hit_result, &anchor) !=
          AR_SUCCESS) {
        LOGE(
            "HelloArApplication::OnTouched ArHitResult_acquireNewAnchor error");
        return;
      }

      ArTrackingState tracking_state = AR_TRACKING_STATE_STOPPED;
      ArAnchor_getTrackingState(ar_session_, anchor, &tracking_state);
      if (tracking_state != AR_TRACKING_STATE_TRACKING) {
        ArAnchor_release(anchor);
        return;
      }

      if (anchor_) {
        ArAnchor_release(anchor_);
      }

      anchor_ = anchor;

      ArHitResult_destroy(ar_hit_result);
      ar_hit_result = nullptr;

      ArHitResultList_destroy(hit_result_list);
      hit_result_list = nullptr;
    }
  }
}
}  // namespace hello_ar
