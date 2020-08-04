

# This is a modified version of product/full_base.mk
# we don't want all that stuff

PRODUCT_PACKAGES := \
    libfwdlockengine \
    OpenWnn \
    libWnnEngDic \
    libWnnJpnDic \
    libwnndict \
    WAPPushManager

PRODUCT_PACKAGES += \
    Galaxy4 \
    HoloSpiralWallpaper \
    LiveWallpapers \
    LiveWallpapersPicker \
    MagicSmokeWallpapers \
    NoiseField \
    PhaseBeam \
    PhotoTable

# Additional settings used in all AOSP builds
PRODUCT_PROPERTY_OVERRIDES := \
    ro.config.ringtone=Ring_Synth_04.ogg \
    ro.config.notification_sound=pixiedust.ogg

# Put en_US first in the list, so make it default.
PRODUCT_LOCALES := en_US

# Get some sounds
$(call inherit-product-if-exists, frameworks/base/data/sounds/AllAudio.mk)

# Get the TTS language packs
$(call inherit-product-if-exists, external/svox/pico/lang/all_pico_languages.mk)

# Get a list of languages.
$(call inherit-product, $(SRC_TARGET_DIR)/product/locales_full.mk)

##### The following comes from there
# $(call inherit-product, $(SRC_TARGET_DIR)/product/generic_no_telephony.mk)
PRODUCT_PACKAGES := \
    Bluetooth \
    BluetoothMidiService \
    Camera2 \
    Gallery2 \
    Music \
    MusicFX \
    NfcNci \
    OneTimeInitializer \
    Provision \
    SystemUI \
    SysuiDarkThemeOverlay \
    EasterEgg \
    WallpaperCropper

PRODUCT_PACKAGES += \
    clatd \
    clatd.conf \
    pppd \
    screenrecord

PRODUCT_PACKAGES += \
    librs_jni \
    libvideoeditor_jni \
    libvideoeditor_core \
    libvideoeditor_osal \
    libvideoeditor_videofilters \
    libvideoeditorplayer \

PRODUCT_PACKAGES += \
    audio.primary.default \
    local_time.default \
    vibrator.default \
    power.default

PRODUCT_COPY_FILES := \
        frameworks/av/media/libeffects/data/audio_effects.conf:system/etc/audio_effects.conf

PRODUCT_PROPERTY_OVERRIDES += \
    ro.carrier=unknown

$(call inherit-product-if-exists, frameworks/base/data/fonts/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/dancing-script/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/carrois-gothic-sc/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/coming-soon/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/cutive-mono/fonts.mk)
$(call inherit-product-if-exists, external/noto-fonts/fonts.mk)
$(call inherit-product-if-exists, external/roboto-fonts/fonts.mk)
$(call inherit-product-if-exists, external/hyphenation-patterns/patterns.mk)
$(call inherit-product-if-exists, frameworks/base/data/keyboards/keyboards.mk)
$(call inherit-product-if-exists, frameworks/webview/chromium/chromium.mk)


$(call inherit-product, $(SRC_TARGET_DIR)/product/core.mk)

# Overrides
PRODUCT_BRAND := generic
PRODUCT_DEVICE := generic
PRODUCT_NAME := generic_no_telephony

##################################################3
