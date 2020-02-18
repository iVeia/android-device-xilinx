Local Build of OTA Server using NDK

The files here were created so as to allow builds of the OTA Server binary
without requiring a full rebuild/deploy of the OS; the tiny script points
things in the right direction to rebuild the sources where they already
exist in the git repo so it does not interfere with the larger OS build.

*** Note *** to run the resulting binaries on the target, an NDK-specific
library is REQUIRED: libc++_shared.so must be stored in /system/lib64
on the target prior to running the iVeiOTA server.  This library is
conveniently thrown into the ./libs/arm64-v8a directory during the build
(and this is also where our binary build artifacts end up).

To build a "local copy" simply run the bash script as follows (sample
output is shown as well):

./build-for-android

----- Building Host-Side OTA Server for Android

Android NDK: WARNING: Invalid LOCAL_CPP_EXTENSION values: cc
Android NDK: WARNING: Invalid LOCAL_CPP_EXTENSION values: cc
Android NDK: WARNING: Invalid LOCAL_CPP_EXTENSION values: cc
Android NDK: WARNING: Invalid LOCAL_CPP_EXTENSION values: cc
[arm64-v8a] Compile++      : ciVeiOTA <= client.cc
[arm64-v8a] Compile++      : ciVeiOTA <= message.cc
[arm64-v8a] Compile++      : ciVeiOTA <= ota_manager.cc
[arm64-v8a] Compile++      : ciVeiOTA <= socket_interface.cc
[arm64-v8a] Compile++      : ciVeiOTA <= uboot.cc
[arm64-v8a] Compile++      : ciVeiOTA <= support.cc
[arm64-v8a] Compile++      : ciVeiOTA <= debug.cc
[arm64-v8a] Compile++      : ciVeiOTA <= config.cc
[arm64-v8a] Prebuilt       : libc++_shared.so <= <NDK>/sources/cxx-stl/llvm-libc++/libs/arm64-v8a/
[arm64-v8a] Executable     : ciVeiOTA
[arm64-v8a] Install        : ciVeiOTA => libs/arm64-v8a/ciVeiOTA
[arm64-v8a] Compile++      : ciecho <= echo_client.cc
[arm64-v8a] Compile++      : ciecho <= socket_echo_interface.cc
[arm64-v8a] Compile++      : ciecho <= debug.cc
[arm64-v8a] Executable     : ciecho
[arm64-v8a] Install        : ciecho => libs/arm64-v8a/ciecho
[arm64-v8a] Compile++      : iVeiOTA <= server.cc
[arm64-v8a] Compile++      : iVeiOTA <= message.cc
[arm64-v8a] Compile++      : iVeiOTA <= ota_manager.cc
[arm64-v8a] Compile++      : iVeiOTA <= socket_interface.cc
[arm64-v8a] Compile++      : iVeiOTA <= uboot.cc
[arm64-v8a] Compile++      : iVeiOTA <= support.cc
[arm64-v8a] Compile++      : iVeiOTA <= debug.cc
[arm64-v8a] Compile++      : iVeiOTA <= config.cc
[arm64-v8a] Executable     : iVeiOTA
[arm64-v8a] Install        : iVeiOTA => libs/arm64-v8a/iVeiOTA
[arm64-v8a] Compile++      : iecho <= echo_server.cc
[arm64-v8a] Compile++      : iecho <= socket_echo_interface.cc
[arm64-v8a] Compile++      : iecho <= debug.cc
[arm64-v8a] Executable     : iecho
[arm64-v8a] Install        : iecho => libs/arm64-v8a/iecho
[arm64-v8a] Install        : libc++_shared.so => libs/arm64-v8a/libc++_shared.so

----- Android build finished.

-----
Feb 18, 2020
Mark Likness
