idf_component_register(SRCS "main.cpp" 
                       INCLUDE_DIRS "."
                       REQUIRES LovyanGFX esp_lcd_st7701 esp_lcd esp_mm esp_hw_support esp_littlefs led_strip)