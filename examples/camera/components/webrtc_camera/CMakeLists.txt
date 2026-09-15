# driver/gpio.h moved out of the monolithic driver component in ESP-IDF 5.3;
# from 6.0 that component no longer provides it at all.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(gpio_component esp_driver_gpio)
else()
    set(gpio_component driver)
endif()

idf_component_register(
    SRCS
        "webrtc_camera.c"
        "jpeg_streamer.c"
        "camera_controls.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_peer
        esp32-camera
    PRIV_REQUIRES
        cjson
        esp_timer
        esp_wifi
        ${gpio_component}
)
