#!/usr/bin/env bash
echo
echo "----- Building Host-Side OTA Server for Android"
echo

export NDK_PROJECT_PATH=.
ndk-build --always-make NDK_APPLICATION_MK=./Application.mk

# show we completed
echo
echo "----- Android build finished."
echo
