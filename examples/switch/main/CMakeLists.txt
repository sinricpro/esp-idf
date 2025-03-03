idf_component_register(
    SRCS "switch_example.cpp"
    INCLUDE_DIRS "."
    REQUIRES sinricpro_esp_idf esp_wifi nvs_flash driver
)