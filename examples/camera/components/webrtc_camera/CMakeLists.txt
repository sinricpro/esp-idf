set(srcs
    "webrtc_camera.c"
    "jpeg_streamer.c"
    "camera_controls.c"
)
set(requires esp_peer esp32-camera)

# esp_h264 supports the S3 only; other targets stream JPEG over the DataChannel.
if(CONFIG_IDF_TARGET_ESP32S3)
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
