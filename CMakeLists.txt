idf_component_register(
    SRCS 
        "src/sinricpro.cpp"
        "src/sinricpro_signature.cpp"
        "src/sinricpro_websocket.cpp"
        "src/sinricpro_device.cpp"
        "src/sinricpro_switch.cpp"
        "src/timestamp.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "nvs_flash"
        "esp_http_client"
        "esp_websocket_client"
        "json"
        "mbedtls"
        "esp_wifi"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable)