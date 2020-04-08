/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
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

package com.nvidia.ar.hellocloudxr;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.provider.Settings;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

/** Helper to ask permissions we need. */
public class PermissionHelper {
  private static final String CAMERA_PERMISSION = Manifest.permission.CAMERA;
  private static final String INTERNET_PERMISSION = Manifest.permission.INTERNET;
  private static final String WRITE_PERMISSION = Manifest.permission.READ_EXTERNAL_STORAGE;
  private static final String READ_PERMISSION = Manifest.permission.WRITE_EXTERNAL_STORAGE;
  private static final int PERMISSION_CODE = 0;

  /** Check to see we have the necessary permissions for this app. */
  public static boolean hasPermissions(Activity activity) {
    return ContextCompat.checkSelfPermission(activity, CAMERA_PERMISSION)
          == PackageManager.PERMISSION_GRANTED &&
        ContextCompat.checkSelfPermission(activity, INTERNET_PERMISSION)
          == PackageManager.PERMISSION_GRANTED &&
        ContextCompat.checkSelfPermission(activity, WRITE_PERMISSION)
          == PackageManager.PERMISSION_GRANTED &&
        ContextCompat.checkSelfPermission(activity, READ_PERMISSION)
          == PackageManager.PERMISSION_GRANTED;
  }

  /** Check to see we have the necessary permissions for this app, and ask for them if we don't. */
  public static void requestPermissions(Activity activity) {
    ActivityCompat.requestPermissions( activity,
      new String[] { CAMERA_PERMISSION, INTERNET_PERMISSION, WRITE_PERMISSION, READ_PERMISSION },
      PERMISSION_CODE
    );
  }

  /** Check to see if we need to show the rationale for this permission. */
  public static boolean shouldShowRequestPermissionRationale(Activity activity) {
    return ActivityCompat.shouldShowRequestPermissionRationale(activity, CAMERA_PERMISSION) ||
      ActivityCompat.shouldShowRequestPermissionRationale(activity, INTERNET_PERMISSION) ||
      ActivityCompat.shouldShowRequestPermissionRationale(activity, WRITE_PERMISSION) ||
      ActivityCompat.shouldShowRequestPermissionRationale(activity, READ_PERMISSION);
  }

  /** Launch Application Setting to grant permission. */
  public static void launchPermissionSettings(Activity activity) {
    Intent intent = new Intent();
    intent.setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
    intent.setData(Uri.fromParts("package", activity.getPackageName(), null));
    activity.startActivity(intent);
  }
}
