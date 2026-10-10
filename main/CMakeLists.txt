idf_component_register(
    SRCS "app_main.cpp" "imu.cpp" "battery.cpp" "store.cpp" "panel.cpp"
         "../core/sand.cpp" "../core/session.cpp" "../core/font.cpp" "../core/scene.cpp" "../core/menu.cpp"
    INCLUDE_DIRS "." "../core"
    REQUIRES waveshare__esp32_s3_touch_amoled_1_75c waveshare__qmi8658 lvgl__lvgl esp_timer driver nvs_flash
)
