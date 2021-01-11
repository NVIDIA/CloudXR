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

#include "CloudXRClient.h"
#include "CloudXRInputEvents.h"
#include "blitter.h"
#include "CloudXRClientOptions.h"

namespace hello_ar {
namespace {
const glm::vec3 kWhite = {255, 255, 255};
}  // namespace

class ARLaunchOptions : public CloudXR::ClientOptions {
public:
    bool using_env_lighting_;
    float res_factor_;

    bool hosting_cloud_anchor_ = false;
    std::string cloud_anchor_id_;

    ARLaunchOptions() :
      ClientOptions(),
      using_env_lighting_(true), // default ON
      // default to 0.75 reduced size, as many devices can't handle full throughput.
      // 0.75 chosen as WAR value for steamvr buffer-odd-size bug, works on galaxytab s6 + pixel 2
      res_factor_(0.75f)
    {
      AddOption("env-lighting", "e", true, "Send client environment lighting data to server.  1 enables, 0 disables.",
                 HANDLER_LAMBDA_FN
                 {
                    if (tok=="1") {
                      using_env_lighting_ = true;
                    }
                    else if (tok=="0") {
                      using_env_lighting_ = false;
                    }
                    return ParseStatus_Success;
                });
      AddOption("res-factor", "r", true, "Adjust client resolution sent to server, reducing res by factor. Range [0.5-1.0].",
                 HANDLER_LAMBDA_FN
                 {
                    float factor = std::stof(tok);
                    if (factor >= 0.5f && factor <= 1.0f)
                      res_factor_ = factor;
                    return ParseStatus_Success;
                 });
      AddOption("cloud-anchor", "c", true, "Share recorded anchor data in google cloud. Use 'host' to save anchors to cloud, or provide cloud anchor ID to load that anchor set from cloud.",
                 HANDLER_LAMBDA_FN
                 {
                    if (tok=="host") {
                      hosting_cloud_anchor_ = true;
                    } else {
                      hosting_cloud_anchor_ = false;
                      cloud_anchor_id_ = tok;
                    }
                    return ParseStatus_Success;
                 });
    }
};

class HelloArApplication::CloudXRClient {
 public:
  ~CloudXRClient() {
    Teardown();
  }

  void TriggerHaptic(const cxrHapticFeedback*) {}
  void GetTrackingState(cxrVRTrackingState* state) {
    *state = {};

    state->hmd.pose.poseIsValid = cxrTrue;
    state->hmd.pose.deviceIsConnected = cxrTrue;
    state->hmd.pose.trackingResult = cxrTrackingResult_Running_OK;

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      const int idx = current_idx_ == 0 ?
          kQueueLen - 1 : (current_idx_ - 1)%kQueueLen;
      state->hmd.pose.deviceToAbsoluteTracking = hmd_matrix_[idx];
    }
  }

  cxrDeviceDesc GetDeviceDesc() {
    device_desc_.deliveryType = cxrDeliveryType_Mono_RGBA;
    device_desc_.width = stream_width_;
    device_desc_.height = stream_height_;
    device_desc_.maxResFactor = 1.0f; // leave alone, don't extra oversample on server.
    device_desc_.fps = static_cast<float>(fps_);
    device_desc_.ipd = 0.064f;
    device_desc_.predOffset = -0.02f;
    device_desc_.audio = 1;
    device_desc_.disablePosePrediction = false;
    device_desc_.angularVelocityInDeviceSpace = false;

    return device_desc_;
  }

  void Connect() {
    if (cloudxr_receiver_)
      return;

    LOGI("Connecting to CloudXR at %s...", launch_options_.mServerIP.c_str());

    cxrGraphicsContext context{cxrGraphicsContext_GLES};
    context.egl.display = eglGetCurrentDisplay();
    context.egl.context = eglGetCurrentContext();

    auto device_desc = GetDeviceDesc();

    cxrClientCallbacks clientProxy = { 0 };
    clientProxy.GetTrackingState = [](void* context, cxrVRTrackingState* trackingState)
    {
        return reinterpret_cast<CloudXRClient*>(context)->GetTrackingState(trackingState);
    };
    clientProxy.TriggerHaptic = [](void* context, const cxrHapticFeedback* haptic)
    {
        return reinterpret_cast<CloudXRClient*>(context)->TriggerHaptic(haptic);
    };

    cxrReceiverDesc desc = { 0 };
    desc.requestedVersion = CLOUDXR_VERSION_DWORD;
    desc.deviceDesc = device_desc;
    desc.clientCallbacks = clientProxy;
    desc.clientContext = this;
    desc.shareContext = &context;
    desc.numStreams = 2;
    desc.receiverMode = cxrStreamingMode_XR;
    desc.debugFlags = launch_options_.mDebugFlags;
    desc.logMaxSizeKB = CLOUDXR_LOG_MAX_DEFAULT;
    desc.logMaxAgeDays = CLOUDXR_LOG_MAX_DEFAULT;
    cxrError err = cxrCreateReceiver(&desc, &cloudxr_receiver_);
    if (err != cxrError_Success)
    {
      LOGE("Failed to create CloudXR receiver. Error %d, %s.", err, cxrErrorString(err));
      return; // err;
    }
    err = cxrConnect(cloudxr_receiver_, launch_options_.mServerIP.c_str());
    if (err != cxrError_Success)
    {
      LOGE("Failed to connect to CloudXR server at %s. Error %d, %s.", launch_options_.mServerIP.c_str(), (int)err, cxrErrorString(err));
      Teardown();
      return; // err;
    }

    // else, good to go.
    LOGI("Receiver created!");

    // AR shouldn't have an arena, should it?  Maybe something large?
    //LOGI("Setting default 1m radius arena boundary.", result);
    //cxrSetArenaBoundary(Receiver, 10.f, 0, 0);
  }

  void Teardown() {
    if (cloudxr_receiver_) {
      LOGI("Tearing down CloudXR...");
      cxrDestroyReceiver(cloudxr_receiver_);
    }

    cloudxr_receiver_ = nullptr;
  }

  bool IsRunning() const {
    return cloudxr_receiver_ && cxrIsRunning(cloudxr_receiver_);
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

      device_desc_.proj[0][0] = l;
      device_desc_.proj[0][1] = r;
      device_desc_.proj[0][2] = -t;
      device_desc_.proj[0][3] = -b;
    } else {
      // Symmetric projection
      device_desc_.proj[0][0] = -1.f/projection[0][0];
      device_desc_.proj[0][1] = -device_desc_.proj[0][0];
      device_desc_.proj[0][2] = -1.f/projection[1][1];
      device_desc_.proj[0][3] = -device_desc_.proj[0][2];
    }

    device_desc_.proj[1][0] = device_desc_.proj[0][0];
    device_desc_.proj[1][1] = device_desc_.proj[0][1];

    // Disable right eye rendering
    device_desc_.proj[1][2] = 0;
    device_desc_.proj[1][3] = 0;

    LOGI("Proj: %f %f %f %f", device_desc_.proj[0][0], device_desc_.proj[0][1],
        device_desc_.proj[0][2], device_desc_.proj[0][3]);
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
    const bool have_frame = cxrLatchFrameXR(cloudxr_receiver_,
        &frame_, timeout_ms) == cxrError_Success;

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

    cxrReleaseFrameXR(cloudxr_receiver_, &frame_);
    latched_ = false;
  }

  void Render(const float color_correction[4]) {
    if (!IsRunning() || !latched_) {
      return;
    }

    blitter_.BlitTexture(0, 0, 0, 0, 0,
        (uint32_t)frame_.eyeTexture[0].texture,
        (uint32_t)frame_.eyeTexture[1].texture,
        color_correction);
  }

  void UpdateLightProps(const float primaryDirection[3], const float primaryIntensity[3],
      const float ambient_spherical_harmonics[27]) {
    cxrLightProperties lightProperties;

    for (uint32_t n = 0; n < 3; n++) {
      lightProperties.primaryLightColor.v[n] = primaryIntensity[n];
      lightProperties.primaryLightDirection.v[n] = primaryDirection[n];
    }

    for (uint32_t n = 0; n < CXR_MAX_AMBIENT_LIGHT_SH*3; n++) {
      lightProperties.ambientLightSh[n/3].v[n%3] = ambient_spherical_harmonics[n];
    }

    cxrSendLightProperties(cloudxr_receiver_, &lightProperties);
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

  void SetArgs(const std::string &args) {
    LOGI("App args: %s.", args.c_str());
    launch_options_.ParseString(args);
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
  void SetStreamRes(uint32_t w, uint32_t h, uint32_t orientation) {
      // in portrait modes we want width to be smaller dimension
      if (w > h && (orientation == 0 || orientation == 2)) {
          std::swap(w, h);
      }
      // apply the res factor to width and height, and make sure they are even for stream res.
      stream_width_ = ((uint32_t)round((float)w * launch_options_.res_factor_)) & ~1;
      stream_height_ = ((uint32_t)round((float)h * launch_options_.res_factor_)) & ~1;
      LOGI("SetStreamRes: Display res passed = %dx%d", w, h);
      LOGI("SetStreamRes: Stream res set = %dx%d [max %d]", stream_width_, stream_height_);
  }

  // Send a touch event along to the server/host application
  void HandleTouch(float x, float y) {
    if (!IsRunning()) return;

    cxrInputEvent input;
    input.type = cxrInputEventType_Touch;
    input.event.touchEvent.type = cxrTouchEventType_FINGERUP;
    input.event.touchEvent.x = x;
    input.event.touchEvent.y = y;
    cxrSendInputEvent(cloudxr_receiver_, &input);
  }

  const ARLaunchOptions& GetLaunchOptions() {
    return launch_options_;
  }

private:
  static constexpr int kQueueLen = BackgroundRenderer::kQueueLen;

  cxrReceiverHandle cloudxr_receiver_ = nullptr;

  ARLaunchOptions launch_options_;

  uint32_t stream_width_ = 720;
  uint32_t stream_height_ = 1440;

  cxrVideoFrameXR frame_ = {};
  bool latched_ = false;

  std::mutex state_mutex_;
  cxrMatrix34 hmd_matrix_[kQueueLen] = {};
  cxrDeviceDesc device_desc_ = {};
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

// pass command line args direct to client.
void HelloArApplication::SetArgs(const std::string &args) {
  cloudxr_client_->SetArgs(args);
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

    ArAugmentedImageDatabase* ar_augmented_image_database = nullptr;

    if (FILE* f = fopen("/sdcard/image_anchors.imgdb", "rb"))
    {
      LOGI("Image anchors DB found.");

      fseek(f, 0, SEEK_END);
      size_t db_size = ftell(f);
      fseek(f, 0, SEEK_SET);

      uint8_t* raw_buffer = new uint8_t[db_size];
      fread(raw_buffer, 1, db_size, f);
      fclose(f);

      const ArStatus status = ArAugmentedImageDatabase_deserialize(
          ar_session_, raw_buffer, db_size, &ar_augmented_image_database);

      if (status != AR_SUCCESS) {
        LOGI("Unable to deserialize image anchors DB!");
      }

      delete [] raw_buffer;
    }

      ArConfig* config = nullptr;
      ArConfig_create(ar_session_, &config);
      ArSession_getConfig(ar_session_, config);

    if (cloudxr_client_->GetUseEnvLighting()) {
      ArConfig_setLightEstimationMode(ar_session_, config,
          AR_LIGHT_ESTIMATION_MODE_ENVIRONMENTAL_HDR);
    }

    if (ar_augmented_image_database) {
      ArConfig_setAugmentedImageDatabase(ar_session_, config,
                                         ar_augmented_image_database);
      using_image_anchors_ = true;
      LOGI("Using image anchors.");

      ArAugmentedImageDatabase_destroy(ar_augmented_image_database);
    }

    // Enable cloud anchors when necessary
    if (cloudxr_client_->GetLaunchOptions().hosting_cloud_anchor_ ||
        !cloudxr_client_->GetLaunchOptions().cloud_anchor_id_.empty())
    {
      ArConfig_setCloudAnchorMode(ar_session_, config,
          AR_CLOUD_ANCHOR_MODE_ENABLED);
      LOGI("Enabling cloud anchors.");
    }

      ArSession_configure(ar_session_, config);
      ArConfig_destroy(config);
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
  LOGI("OnSurfaceChanged(%d, %d, %d)", display_rotation, width, height);
  glViewport(0, 0, width, height);
  display_rotation_ = display_rotation;
  display_width_ = width;
  display_height_ = height;
  if (ar_session_ != nullptr) {
    ArSession_setDisplayGeometry(ar_session_, display_rotation, width, height);
  }
  cloudxr_client_->SetStreamRes(display_width_, display_height_, display_rotation);
}

void HelloArApplication::UpdateImageAnchors() {
  if (!using_image_anchors_)
    return;

  ArTrackableList* updated_image_list = nullptr;
  ArTrackableList_create(ar_session_, &updated_image_list);
  CHECK(updated_image_list != nullptr);
  ArFrame_getUpdatedTrackables(
      ar_session_, ar_frame_, AR_TRACKABLE_AUGMENTED_IMAGE, updated_image_list);

  int32_t image_list_size;
  ArTrackableList_getSize(ar_session_, updated_image_list, &image_list_size);

  // Find newly detected image, add it to map
  for (int i = 0; i < image_list_size; ++i) {
    ArTrackable* ar_trackable = nullptr;
    ArTrackableList_acquireItem(ar_session_, updated_image_list, i,
                                &ar_trackable);
    ArAugmentedImage* image = ArAsAugmentedImage(ar_trackable);

    ArTrackingState tracking_state;
    ArTrackable_getTrackingState(ar_session_, ar_trackable, &tracking_state);

    int image_index;
    ArAugmentedImage_getIndex(ar_session_, image, &image_index);

    switch (tracking_state) {
      case AR_TRACKING_STATE_PAUSED:
        // When an image is in PAUSED state but the camera is not PAUSED,
        // that means the image has been detected but not yet tracked.
        LOGI("Detected Image %d", image_index);
        break;
      case AR_TRACKING_STATE_TRACKING:
        if (augmented_image_map.find(image_index) ==
            augmented_image_map.end()) {
          // Record the image and its anchor.
          util::ScopedArPose scopedArPose(ar_session_);
          ArAugmentedImage_getCenterPose(ar_session_, image,
                                         scopedArPose.GetArPose());

          ArAnchor* image_anchor = nullptr;
          const ArStatus status = ArTrackable_acquireNewAnchor(
              ar_session_, ar_trackable, scopedArPose.GetArPose(),
              &image_anchor);
          CHECK(status == AR_SUCCESS);

          // Now we have an Anchor, record this image.
          augmented_image_map[image_index] =
              std::pair<ArAugmentedImage*, ArAnchor*>(image, image_anchor);
        }
        break;

      case AR_TRACKING_STATE_STOPPED: {
        std::pair<ArAugmentedImage*, ArAnchor*> record =
            augmented_image_map[image_index];
        ArTrackable_release(ArAsTrackable(record.first));
        ArAnchor_release(record.second);
        augmented_image_map.erase(image_index);
      } break;

      default:
        break;
    }  // End of switch (tracking_state)
  }    // End of for (int i = 0; i < image_list_size; ++i) {

  ArTrackableList_destroy(updated_image_list);
  updated_image_list = nullptr;

  if (!base_frame_calibrated_ && !augmented_image_map.empty()) {
    anchor_ = augmented_image_map.begin()->second.second;
    base_frame_calibrated_ = true;
  }
}

void HelloArApplication::UpdateCloudAnchor() {
  if (using_image_anchors_)
    return;

  const auto& launch_options = cloudxr_client_->GetLaunchOptions();

  if (!launch_options.hosting_cloud_anchor_ &&
      launch_options.cloud_anchor_id_.empty()) {
    return;
  }

  // Lazy creation
  if (launch_options.hosting_cloud_anchor_ && base_frame_calibrated_ &&
      nullptr != anchor_ && nullptr == cloud_anchor_) {

    ArTrackingState tracking_state = AR_TRACKING_STATE_STOPPED;
    ArAnchor_getTrackingState(ar_session_, anchor_, &tracking_state);

    if (tracking_state == AR_TRACKING_STATE_TRACKING) {
      if (auto status = ArSession_hostAndAcquireNewCloudAnchor(ar_session_,
          anchor_, &cloud_anchor_)) {
        LOGE("Cloud anchor hosting failed with %d.", status);
      }
    }
  } else if (!launch_options.cloud_anchor_id_.empty() &&
      (nullptr == anchor_ || !base_frame_calibrated_)) {
    if (auto status = ArSession_resolveAndAcquireNewCloudAnchor(ar_session_,
        launch_options.cloud_anchor_id_.c_str(), &anchor_)) {
      LOGE("Cloud anchor resolve failed with %d.", status);
    } else {
      LOGI("Cloud anchor \"%s\" resolved!",
          launch_options.cloud_anchor_id_.c_str());

      base_frame_calibrated_ = true;
      cloud_anchor_ = anchor_;
    }
  }

  // Grab state
  ArCloudAnchorState state = AR_CLOUD_ANCHOR_STATE_NONE;
  if (nullptr != cloud_anchor_) {
    ArAnchor_getCloudAnchorState(ar_session_, cloud_anchor_, &state);
  } else {
    // No cloud anchor around - bail
    return;
  }

  if (state != AR_CLOUD_ANCHOR_STATE_SUCCESS) {
    if (state == AR_CLOUD_ANCHOR_STATE_TASK_IN_PROGRESS) {
      LOGI("Cloud anchor in progress...");
    } else if (state >= AR_CLOUD_ANCHOR_STATE_ERROR_INTERNAL) {
      LOGE("Cloud anchor error state %d.", state);
    }
    return;
  }

  // Get hosted id and report
  // TODO(iterentiev): find a better way to report
  if (launch_options.hosting_cloud_anchor_ && nullptr != cloud_anchor_) {
    char* cloud_anchor_id = nullptr;
    ArAnchor_acquireCloudAnchorId(ar_session_, cloud_anchor_,
        &cloud_anchor_id);

    LOGI("Hosted cloud anchor id: %s.", cloud_anchor_id);

    ArString_release(cloud_anchor_id);
  }
}

// Render the scene.
void HelloArApplication::OnDrawFrame() {
  // clearing to dark red to start, so it is obvious if we fail out early or don't render anything
  glClearColor(0.3f, 0.0f, 0.0f, 1.0f);
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
    if (camera_tracking_state == AR_TRACKING_STATE_STOPPED)
    {
      LOGI("Note camera tracking is in STOPPED state.");
    }
    else
    { // camera is paused state
      LOGI("Note camera tracking is PAUSED.");
      ArTrackingFailureReason reason;
      ArCamera_getTrackingFailureReason(ar_session_, ar_camera, &reason);
      switch(reason)
      {
          case AR_TRACKING_FAILURE_REASON_NONE:
              break;
          case AR_TRACKING_FAILURE_REASON_BAD_STATE:
              LOGE("Camera tracking lost due to bad internal state.");
              break;
          case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_LIGHT:
              LOGE("Camera tracking lost due to insufficient lighting.  Please move to brighter area.");
              break;
          case AR_TRACKING_FAILURE_REASON_EXCESSIVE_MOTION:
              LOGE("Camera tracking lost due to excessive motion.  Please move more slowly.");
              break;
          case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_FEATURES:
              LOGE("Camera tracking lost due to insufficient visual features to track.  Move to area with more surface details.");
              break;
      }
    }
    return;
  }

  // We need to (re)calibrate but CloudXR client is running - continue
  // pulling the frames. There'll be a lag otherwise.
  if (!base_frame_calibrated_ && cloudxr_client_->IsRunning()) {
    if (cloudxr_client_->Latch())
      cloudxr_client_->Release();
  }

  UpdateImageAnchors();
  UpdateCloudAnchor();

  if (base_frame_calibrated_ &&
      !cloudxr_client_->GetLaunchOptions().hosting_cloud_anchor_) {
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
  if (!cloudxr_client_->GetLaunchOptions().hosting_cloud_anchor_ &&
      (base_frame_calibrated_ || using_image_anchors_)) {
    return;
  }

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
      LOGE("Tracked plane lost, skipping drawing.");
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
  // if base frame is calibrated and user is not asking to reset, pass touches to server
  if (base_frame_calibrated_ && !longPress) {
    if (cloudxr_client_->IsRunning())
      cloudxr_client_->HandleTouch(x, y);
    return;  // TODO: should return true/false for handled/used the event...
  }

  // Do not ever recalibrate when hosting an anchor
  if (base_frame_calibrated_ &&
      cloudxr_client_->GetLaunchOptions().hosting_cloud_anchor_) {
    return;
  }

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
