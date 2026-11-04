idf_component_register(
    SRCS "esp_hi_rebind.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_hw_support
)
