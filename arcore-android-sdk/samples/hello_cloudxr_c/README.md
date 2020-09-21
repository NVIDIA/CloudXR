NVIDIA CloudXR ARCore sample
====================================================

NVIDIA CloudXR sample application for ARCore. Heavily based on hello_ar_c ARCore sample application from ARCore SDK.

Prerequisites
----------------------
* Android Studio v3.4 or later. [Link](https://developer.android.com/studio)
  * (optional) A fully setup Android development environment with Android SDK, Android NDK and Android Platform Tools and OpenJDK 1.8
* CloudXR 1.2 SDK Release

Building with Android Studio
----------------------
* Copy CloudXR SDK Android client package, "CloudXR.aar", from [CloudXRSDK]/Client/Lib/Android folder) into [hello_cloudxr_c]/libs folder.
* Launch Android Studio and open [hello_cloudxr_c] as an existing project.
* Make the project
    * If build fails with an error about manifest, go to File, Settings, Experimental (all the way at bottom of left list), and UNCHECK “Only sync the active variant” box.

Building with Gradle (optional)
----------------------
* Copy CloudXR SDK Android client package, "CloudXR.aar", from [CloudXRSDK]/Client/Lib/Android folder) into [hello_cloudxr_c]/libs folder.
* Execute Gradle build ("gradlew build") in the [hello_cloudxr_c] project folder

Installing
----------------------
* Enable developer mode on the target device
* Connect the target device to the workstation
* Setup ADB connection
* From Android Studio, launch the application on target device 
  * (optional) If building with Gradle, you can use it to install via "gradlew installRelease"

Running
----------------------
* Start SteamVR on the workstation
* Start "CloudXR ARCore Client" application on the device
    * Give the application permissions as requested
    * Enter CloudXR server ip-address
    * The camera view will start on the device, and you will now calibrate the AR view.
    * Point your device at a surface you want to use as your ground plane.
    * Slowly pan the device left and right, up and down, back and forth (repeatedly), until you see a dotted mesh start to appear.  As you continue to pan the device, the mesh should fill out until you have a sufficient area covered.
    * Touch a point on the screen to use as the AR anchor.
    * The application should now connect to the server and show the default SteamVR view.
* To view AR, you need an application with a VR stream where the left eye is the 3D scene and the right eye is the Alpha channel.
    * On your server/workstation, in the /TestTools folder in the root of the CloudXR SDK, you will find a sample application `ar_test.exe`.  If you launch that, you should see a three dimensional grid of boxes merged with the camera view.
* Press and hold on the screen to reset the calibration, and re-scan the environment.


Optional
----------------------
* To connect to server automatically
    * Using a 'launch options' file on device.
        * Create a text file on development PC named CloudXRLaunchOptions.txt
        * Add the server IP to the file in format similar to a commandline argument:
            * `-s [server ip]`
        * Then copy the file to the root folder of device sdcard
            * via MTP just drag and drop to root
            * or using ADB:
                * `adb push CloudXRLaunchOptions.txt /sdcard/CloudXRLaunchOptions.txt`
    * Using commandline argument from Android Studio
        * In Run/Debug Configurations you can set server in "Launch Flags", using:
            * `--es args "-s [server ip]"`
* Other launch options you can specify in the text file or the launch cmdline/flags include:
    * -v
        * Enables more verbose logging
    * -r [res-factor]
        * Allows adjusting the target stream resolution as a factor of display resolution.
        * The default value is 0.75, so default stream res will be 0.75 * (device res).
        * The allowed range for factor is 0.5 to 1.0 multiplier.
    * -e [1|0]
        * Enable/disable environmental lighting support.
        * Default is on, if performance issues try turning off.

License
----------------------
* [License](license.txt)
