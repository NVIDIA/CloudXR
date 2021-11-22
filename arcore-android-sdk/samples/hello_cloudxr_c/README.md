NVIDIA CloudXR ARCore sample
============================

NVIDIA CloudXR sample application for ARCore. Heavily based on hello_ar_c sample application from ARCore SDK.

Please see the full [CloudXR online documentation](https://docs.nvidia.com/cloudxr-sdk/index.html) for latest updated details on this sample and the overall SDK.

Prerequisites
-------------

* An Android tablet or phone that supports the ARCore SDK
* CloudXR 3.1 SDK Release
* [Android Studio v4.0 or later](https://developer.android.com/studio)
  * (optional) A fully setup Android development environment with Android SDK, Android NDK, and Android Platform Tools and OpenJDK 1.8
* Base required version of tools: Android SDK 25, NDK 21.4.7075529, Build tools 28.0.3, Gradle 6.1.1, and Gradle android plugin 3.6.4.
* [Google Oboe SDK 1.5.0](https://github.com/google/oboe/releases/tag/1.5.0)
* [ARCore SDK 1.26.0 or newer](https://developers.google.com/ar)
> NOTE: Since build.gradle has ARCore listed as a dependency, the gradle build will automatically download and cache the SDK for you.  You may however find you need to adjust the ***version number*** higher based on the devices and features that you support.  This sample is currently based on ARCore SDK 1.26.0.

Building with Android Studio
----------------------------

* Copy Google Oboe SDK .AAR file, `oboe-1.5.0.aar` into `[hello_cloudxr_c]/libs` folder.
* Copy CloudXR SDK Android client package, `CloudXR.aar`, from `[CloudXRSDK]/Client/Lib/Android` folder into `[hello_cloudxr_c]/libs` folder.
* Launch Android Studio and open [hello_cloudxr_c] as an existing project.
* Build>Make the project

>If build fails with an error about manifest, go to File, Settings, Experimental (all the way at bottom of left list), and ***uncheck*** “Only sync the active variant” box.

Building with Gradle (optional)
-------------------------------

* Copy Google Oboe SDK .AAR file, `oboe-1.5.0.aar` into `[hello_cloudxr_c]/libs` folder.
* Copy CloudXR SDK Android client package, `CloudXR.aar`, from [CloudXRSDK]/Client/Lib/Android folder) into `[hello_cloudxr_c]/libs` folder.
* Execute Gradle build (``gradlew build``) in the `[hello_cloudxr_c]` project folder

Installing
----------

* Place the device in developer mode and allow USB connection in debug mode on the device
* Use a USB cable to connect the Android device to the development system
* If prompted on the device to allow connections, select **Allow**.
* Install the APK by:
  * Calling adb directly in console: `adb.exe install -r <APK_name>.apk`
  * If building with Grade, can use: `gradlew installRelease`
  * In Android Studio, simply Run the app, it will install as needed.

Running
-------

* Start **SteamVR** on the server system
* Start **CloudXR ARCore Client** application on the Android device
* If prompted, allow requested permissions
* When prompted, enter the IP address of the CloudXR server or accept a previously entered value
* The camera view will start on the device, and you will now calibrate the AR view.
    * Point your device at a surface you want to use as your ground plane.
    * Slowly pan the device left and right, up and down, back and forth (repeatedly), until you see a dotted mesh start to appear.  As you continue to pan the device, the mesh should fill out until you have a sufficient area covered.
    * Touch the screen to select the anchor point for the AR world.
* Wait a moment as the app connects to the server, and shows the default SteamVR view (or whatever application was already running on the server).
* To reset the anchored point in the camera view, simply press and hold your finger on the screen for a few seconds.  That will reset the anchored position and allow you to set a new position.

> To stream CloudXR with AR content, the server application, which is an OpenVR app, provides the main scene in the left eye containing RGBA data. The alpha channel in the data indicates the regions that should be blended with live camera content. The right eye data is ignored, but because OpenVR requires the submission of both eyes, we recommend for best performance that the application either submit the left eye texture also as the right eye texture (using the same texture handle) or optionally submit a small dummy texture for the right eye.
>
>A very basic server application `ar_test.exe` is provided to help illustrate and validate AR streaming.  Look in the `[CloudXRSDK]\TestTools` folder.  Start `ar_test.exe` on the server after connection to see an array of textured cubes blended into the current camera view.

Optional
--------

* To connect to server automatically, Create a plain-text file named `CloudXRLaunchOptions.txt` that contains `-s <IP-address-CloudXR-server>` for example, for a server with *IP = 192.168.1.1* the file should contain `-s 192.168.1.1`.
* Then copy the file to the root folder of device sdcard
    * via MTP/Explorer just drag and drop to root of the sdcard
    * or using ADB from command prompt, use `adb push CloudXRLaunchOptions.txt /sdcard/CloudXRLaunchOptions.txt`
* If started from Android Studio, in Run/Debug Configurations you can specify command line arguments in Launch Flags, using `--es args "cmdline-string"`. So for the above sever, you would use `--es args "-s 192.168.1.1"`.
* Other launch options you can specify in text file or launch flags include:
    * `-v`
        * Enables more verbose logging
    * `-rf [factor]`
        * Allows reducing video stream resolution to a factor of display resolution.
        * The default value is 0.75, so default stream res will be 0.75 * (device res).
        * The allowed range for factor is 0.5 to 1.0 multiplier.
    * `-el [on|off]`
        * Enable/disable environmental lighting support.
        * Default is on, if performance issues try turning off.
    * `-ca [host|<cloud anchor ID>]`
        * Enable sharing recorded anchor data via Google ARCore Cloud Anchor support
        * Use 'host' to save anchors to cloud, or provide cloud anchor ID to load that anchor set from cloud.
NOTE: For more information on using launch options and a full list of all available options, see the ***Command-Line Options*** section of the online CloudXR documentation.

License
----------------------

* [License](license.txt)
