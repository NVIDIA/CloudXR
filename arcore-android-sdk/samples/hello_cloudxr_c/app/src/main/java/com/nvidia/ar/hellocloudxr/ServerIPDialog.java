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

import android.app.Activity;
import android.app.AlertDialog;
import android.widget.EditText;
import android.util.Patterns;
import android.content.DialogInterface;
import android.widget.Toast;

// Adapted from https://twigstechtips.blogspot.com/2011/10/android-allow-user-to-editinput-text.html
public class ServerIPDialog {
  public static void show(HelloArActivity activity, String prevIp) {
    final HelloArActivity thiz = activity;
    final EditText serverIp = new EditText(activity);

    serverIp.setHint("127.0.0.1");
    serverIp.setText(prevIp);

    new AlertDialog.Builder(activity)
        .setTitle("CloudXR Server IP")
        .setMessage("Please enter CloudXR server IP address.")
        .setView(serverIp)
        .setPositiveButton("Go", new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int whichButton) {
            String ip = serverIp.getText().toString();

            if (Patterns.IP_ADDRESS.matcher(ip).matches()) {
                thiz.setServerIp(ip);
                thiz.doResume();
            } else {
              Toast.makeText(thiz, "Invalid IP address. Try again.", Toast.LENGTH_SHORT).show();
              ServerIPDialog.show(thiz, prevIp);
            }
          }
        })
        .setNegativeButton("Exit", new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int whichButton) {
            android.os.Process.killProcess(android.os.Process.myPid());
          }
        })
        .show();
  }
}
