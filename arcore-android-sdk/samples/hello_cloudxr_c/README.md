NVIDIA CloudXR ARCore client sample
===================================

This a sample ARCore client for AR streaming via NVIDIA CloudXR.  It is designed to work with Android tablets and phones that support the `ARCore SDK <https://developers.google.com/ar>`_. The client collects motion data from the Android device and sends it to the CloudXR server as 'pose' data. The server renders scene frames based on the client position, and streams them to the client.  The client decodes the video (and alpha when provided) streamed from the CloudXR server and blends it with the Android device's camera view.

> :information_source: **Note**  
> There is nothing preventing a non-AR scene, call it "2D-VR", where you are in a 3D space, you can 'look around' with the camera, but moving/translocating in the space would require some custom coding for user input. 

This sample was started based on the hello_ar_c sample application from the ARCore SDK.

For more details on the overall CloudXR SDK, see the [CloudXR online documentation](https://docs.nvidia.com/cloudxr-sdk/index.html).

ARCore Client Prerequisites
---------------------------

* An Android tablet or phone that supports the ARCore SDK and is capable of 4K HEVC decoding at 60fps or higher. Some example devices include:
  * Samsung Galaxy S21, S22, S23
  * Google Pixel 5, 6A, 7, 8
* [CloudXR 4.0 SDK Release](https://developer.nvidia.com/cloudxr-sdk)
* [Android Studio v4.0 or later](https://developer.android.com/studio)
  * A fully setup Android development environment with Android SDK, Android NDK, and Android Platform Tools and OpenJDK 1.8
* Base required version of tools: Android SDK 25, NDK 21.4.7075529, Build tools 28.0.3, Gradle 6.1.1, and Gradle android plugin 3.6.4.
* [Google Oboe SDK 1.5.0](https://github.com/google/oboe/releases/tag/1.5.0)
* [ARCore SDK 1.26.0 or newer](https://developers.google.com/ar)
  A *.zip* file of the `ARCore SDK <https://developers.google.com/ar>`_.  The sample has historically been based on ARCore SDK 1.26.0, but should work with newer releases.

  As of this writing, you can find the [older 1.26.0 SDK here](https://github.com/google-ar/arcore-android-sdk/releases/download/v1.26.0/arcore-android-sdk-1.26.0.zip)

  > :information_source: **Note**  
  > The gradle project has ARCore as a dependency, so the project will automatically download and cache certain SDK headers and libraries.  However, you might need to adjust the dependent *version number* based on the devices that you need to support, and the manual SDK you download.

Building the ARCore Client
--------------------------

In the below instructions ``{cxr-root}`` is the path to the CloudXR SDK.

> :information_source: **Note**  
> On Windows you may need to move the SDK to the top level of your drive due to path length restrictions. You'll get strange errors from the build process about files not existing, but will also note that the paths being shown are extremely long.

* Unzip the ARCore SDK into the ``{cxr-root}\Sample\Android\GoogleAR`` folder, resulting in the contents being in a subfolder like ``{cxr-root}\Sample\Android\GoogleAR\arcore-android-sdk-1.26.0``.  To simplify next steps, we will use ``{arcore-root}`` to refer to that directory.

* Go to https://github.com/NVIDIA/CloudXR/tree/master/arcore-android-sdk/samples/hello_cloudxr_c and download the entire *hello_cloudxr_c* project and place it in ``{arcore-root}\samples``, at the same level as the ARCore SDK samples.

* Copy the Google Oboe SDK .AAR file (``oboe-1.5.0.aar``) into the ``{arcore-root}\samples\hello_cloudxr_c\libs`` folder.

* Copy the CloudXR SDK client package, which is the ``CloudXR.aar`` file, from ``{cxr-root}\Client\Lib\Android`` to the ``{arcore-root}\samples\hello_cloudxr_c\libs``.

* Run Android Studio.  

* Complete one of the following tasks:

   * Select **Open existing Android Studio project** on the Welcome screen.
   * Click **File** > **Open**.  

* Navigate to ``{arcore-root}\samples`` and select the ``hello_cloudxr_c`` project/folder to open.

* Select **Build > Make Project**.

This process should generate an *.apk* file in the ``{arcore-root}\samples\hello_cloudxr_c\build\outputs\apk\debug`` directory that can be used to debug or be installed manually.

  > :information_source: **Note**  
  > If build fails with an error about manifest, in the lower-left side, select **File** > **Settings** > **Experimental** and deselect the **Only sync the active variant** checkbox.

  > :information_source: **Note**  
  > To build from the command line, run ``gradlew build`` from a command prompt in the ``hello_cloudxr_c`` folder.

* Prepare the device for running/debugging

  * Place the device in developer mode and allow USB connection in debug mode on the device.

  * Use a USB cable to connect the Android device to the development system.

  * If prompted on the device to allow connections, select **Allow**.

Installing the ARCore Client
----------------------------

> :information_source: **Note**  
> This section is only necessary should you want to manually install from command-line.  If running through Android Studio, it will take care of the installation, so you can skip ahead to `Running the ARCore Client`.

* In a Command Prompt window, navigate to the folder that contains the *.apk* file that was created by building the sample application.

* Use ADB to install the application from the release *.apk* file.

  ``adb.exe install -r <APK name>.apk``

  > :information_source: **Note**  
  > By default, the ``ADB.exe`` program is installed by Android Studio in ``C:\Users\{username}\AppData\Local\Android\Sdk\platform-tools``

  When the installation is complete, ADB responds with a *Success* message.

* If you are building from the command-line with Gradle, it can do the installation: `gradlew installRelease`

Running the ARCore Client
-------------------------

> :information_source: **Note**  
> See [Command-Line Options](https://docs.nvidia.com/cloudxr-sdk/usr_guide/cmd_line_options.html) for more information about our command-line/launch options system and a complete list of all the available options, and [How to Launch on Android](https://docs.nvidia.com/cloudxr-sdk/prog_guide/misc_features.html#how-to-launch-on-android) for more detailed instructions on the various methods for starting Android applications.

* Start *SteamVR* or the [CloudXR Experimental Server Sample](https://docs.nvidia.com/cloudxr-sdk/usr_guide/cxr_test_server.html#cloudxr-experimental-server-sample) on the server system.

* Launch the **CloudXR ARCore Client** app on your Android device (via whichever method you choose).

* When prompted, enter the IP address of the CloudXR server or accept a previously entered value.

* Once you see the camera view, you should slowly turn the camera from side to side or up and down until it sufficiently visualizes the target 'ground plane' on the screen as a series of connected white dots.

* Touch the grid to select an anchor point for the AR world.

* Wait until the app connects to the server, and starts streaming.

* For SteamVR, you will get whatever is configured as your home view, or whatever VR application was already running on the server.

  In the ``{cxr-root}\TestTools`` folder, a basic OpenVR server application, ``ar_test.exe``, is provided to help illustrate and validate AR streaming in SteamVR.  After the connection is established, start ``ar_test.exe`` on the server to see a huge 3D grid of textured cubes blended into the current camera view.

  > :information_source: **Note**  
  > To stream CloudXR with AR content, the server application provides the main scene in the left eye that contains the RGBA data. The alpha channel in the data indicates the regions that should be blended with live camera content.
  >
  > The right eye data is ignored, but because OpenVR requires the submission of both eyes we recommend that the application also submit the left eye texture as the right eye texture by using the same texture handle, or optionally submit a small dummy texture for the right eye.

* As an alternative to Steam, you can test AR clients with the experimental Server Sample. More details on the Server Sample and using it can be found in [CloudXR Experimental Server Sample](https://docs.nvidia.com/cloudxr-sdk/usr_guide/cxr_test_server.html#cloudxr-experimental-server-sample). After the connection is established, you will see an angel statue in front of you. Note that the scale is currently a bit large, so you may need to physically take a step back. Then you can move the device around to look at the AR model.

* To reset the anchored point in the camera view, press and hold your finger on the screen for a few seconds.  

  This step resets the anchored position and allows you to scan and set a new root anchor position.

> :information_source: **Note**  
> The ARCore sample has extra command-line options beyond all the defaults.  See [ARCore Client Command-Line Options](https://docs.nvidia.com/cloudxr-sdk/usr_guide/cmd_line_options.html#arcore-client-command-line-options) for more information.

ARCore Client License
---------------------

* See the [License](license.txt) file.
