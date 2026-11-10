idf_component_register(
    SRCS "p4_camera.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_i2c esp_driver_isp esp_driver_cam esp_driver_jpeg esp_timer esp_mm)
