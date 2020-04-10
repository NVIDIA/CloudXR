NVIDIA CloudXR ARCore sample
====================================================

NVIDIA CloudXR sample application for ARCore. Heavily based on hello_ar_c ARCore sample application from ARCore SDK.

Prerequisites
----------------------
* Android Studio v3.4 or later. [Link](https://developer.android.com/studio).
  * (optional) A fully setup Android development environment with Android SDK, Android NDK and Android Platform Tools and OpenJDK 1.8.

Building with Android Studio
----------------------
* Copy CloudXR SDK client package (CloudXR.aar file from Client/Lib/Android folder) into app/libs folder.
* Open project in Android Studio.
* Make the project.

Building with Gradle (optional)
----------------------
* Copy CloudXR SDK client package (CloudXR.aar file from Client/Lib/Android folder) into app/libs folder.
* Execute Gradle build ("gradlew build") in the project folder.

Installing
----------------------
* Enable developer mode on the target device.
* Connect the target device to the workstation.
* Setup ADB connection.
* Run the application from Android Studio.
  * (optional) If building with Gradle execute install ("gradlew installRelease") in the project folder.
* Give "CloudXR AR Client" application permission to read the external storage.

Running
----------------------
* Start SteamVR on the workstation.
* Start "CloudXR ARCore Client" application on the device.
* Enter CloudXR server ip-address.

Optional
----------------------
* To connect to server automatically, create a file named CloudXRLaunchOptions.txt with IP address of server:
    * `-s <server ip>`
* Then copy the file to the root folder of device sdcard via MTP, or using ADB:
    * `adb push CloudXRLaunchOptions.txt /sdcard/CloudXRLaunchOptions.txt`
* If started from Android Studio, in Run/Debug Configurations you can set server in Launch Flags, using:
    * `--es args "-s <server ip>"`


License
----------------------
* [License](license.txt)
