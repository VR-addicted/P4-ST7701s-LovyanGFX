idf_component_register(SRCS "esp_lcd_st7701.c" 
                            "esp_lcd_st7701_mipi.c"
                            "esp_lcd_st7701_rgb.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver)