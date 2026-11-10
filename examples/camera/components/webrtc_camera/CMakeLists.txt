set(srcs
    "webrtc_camera.c"
    "jpeg_streamer.c"
    "camera_controls.c"
)
set(requires esp_peer)

# The P4's MIPI camera is served through p4_camera, which offers the esp32-camera API.
# IDF_TARGET rather than CONFIG_IDF_TARGET_*: requirements are resolved before sdkconfig is loaded.
if(IDF_TARGET STREQUAL "esp32p4")
    list(APPEND requires p4_camera esp_driver_ppa)
else()
    list(APPEND requires esp32-camera)
endif()

# esp_h264 encodes in software on the S3 and in hardware on the P4; other targets stream JPEG
# over the DataChannel.
if(IDF_TARGET STREQUAL "esp32s3" OR IDF_TARGET STREQUAL "esp32p4")
    list(APPEND srcs "h264_streamer.c")
    list(APPEND requires esp_h264)
endif()

idf_component_register(
    SRCS
        ${srcs}
    INCLUDE_DIRS
        "include"
    REQUIRES
        ${requires}
    PRIV_REQUIRES
        cjson
        esp_driver_gpio
        esp_timer
        esp_wifi
)
