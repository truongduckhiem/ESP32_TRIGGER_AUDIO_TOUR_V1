idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        # --- Thư viện hệ thống ESP-IDF ---
        nvs_flash 
        esp_wifi 
        esp_http_server 
        esp_event 
        driver 
        spiffs 
        
        # --- Thư viện Component của bạn ---
        nmea_parser 
        i2cdev 
        pcf8574 
        hd44780 
        dfplayer
)