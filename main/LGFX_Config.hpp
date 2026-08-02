#pragma once
#include "esp_lcd_types.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_cache.h"
#include "esp_lcd_st7701.h"
#include "driver/ledc.h"

#include "esp_littlefs.h"
#include "esp_vfs.h"
#include "esp_log.h"




class LGFX_ST7701_P4 : public lgfx::LGFX_Device {
    lgfx::Panel_Device* _panel_instance = nullptr;

public:
    lgfx::LGFX_Sprite sprite; 
    esp_lcd_panel_handle_t panel_handle = NULL;
    void* framebuffer = NULL;

    LGFX_ST7701_P4() { }
    
    // In der LGFX_ST7701_P4 Klasse (LGFX_Config.hpp)
    void setBrightness(uint8_t duty) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }

    bool initBus() {
        // 1. LDO Setup (2.5V für MIPI)
        esp_ldo_channel_handle_t ldo_mipi;
        esp_ldo_channel_config_t ldo_cfg = {};
        ldo_cfg.chan_id = 3;
        ldo_cfg.voltage_mv = 2500;
        esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi);

        // 2. MIPI DSI Bus
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
        esp_lcd_dsi_bus_config_t bus_config = {};
        bus_config.bus_id = 0;
        bus_config.num_data_lanes = 2;
        bus_config.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
        bus_config.lane_bit_rate_mbps = 450;
        esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

        // 3. MIPI DSI IO (DBI)
        esp_lcd_panel_io_handle_t io_handle;
        esp_lcd_dbi_io_config_t dbi_config = {};
        dbi_config.virtual_channel = 0;
        dbi_config.lcd_cmd_bits = 8;
        dbi_config.lcd_param_bits = 8;
        esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle);

        // 4. DPI-Timing
        esp_lcd_dpi_panel_config_t dpi_config = {};
        dpi_config.virtual_channel = 0;
        dpi_config.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
        dpi_config.dpi_clock_freq_mhz = 20;
        dpi_config.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888;  // RGB565
        dpi_config.num_fbs = 1;
        dpi_config.video_timing.h_size = 480;
        dpi_config.video_timing.v_size = 800;
        dpi_config.video_timing.hsync_pulse_width = 10;
        dpi_config.video_timing.hsync_back_porch = 80;
        dpi_config.video_timing.hsync_front_porch = 80;
        dpi_config.video_timing.vsync_pulse_width = 10;
        dpi_config.video_timing.vsync_back_porch = 20;
        dpi_config.video_timing.vsync_front_porch = 20;
        dpi_config.flags.use_dma2d = 1;

        // 5. Vendor-Config & Panel
        st7701_vendor_config_t vendor_config = {};
        vendor_config.mipi_config.dsi_bus = mipi_dsi_bus;
        vendor_config.mipi_config.dpi_config = &dpi_config;
        vendor_config.flags.use_mipi_interface = 1;

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = (gpio_num_t)5; // LCD_RST_GPIO
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR ;   //  LCD_RGB_ELEMENT_ORDER_RGB
        
        panel_config.bits_per_pixel = 24;   // 16
        panel_config.vendor_config = &vendor_config;
        
        
        esp_lcd_new_panel_st7701(io_handle, &panel_config, &panel_handle);
        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);

        // Backlight PWM Setup statt einfachem GPIO Level
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
        ledc_timer.timer_num        = LEDC_TIMER_0;
        ledc_timer.duty_resolution  = LEDC_TIMER_8_BIT; // 0-255
        ledc_timer.freq_hz          = 5000;             // 5kHz sind flimmerfrei
        ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
        ledc_timer_config(&ledc_timer);

        ledc_channel_config_t ledc_channel = {};
        ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel        = LEDC_CHANNEL_0;
        ledc_channel.timer_sel      = LEDC_TIMER_0;
        ledc_channel.intr_type      = LEDC_INTR_DISABLE;
        ledc_channel.gpio_num       = (gpio_num_t)23; // Dein Backlight Pin
        ledc_channel.duty           = 127;            // Startet direkt mit 50% Helligkeit
        ledc_channel.hpoint         = 0;
        ledc_channel_config(&ledc_channel);

        // 6. Framebuffer & Sprite Link
        esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 1, &framebuffer);
        if (framebuffer) {
        // ERST die Farbtiefe definieren
            sprite.setColorDepth(24); 
            // DANN den Buffer verknüpfen
            sprite.setBuffer(framebuffer, 480, 800, 24);
            return true;
        }
        return false;
    }

    void pushCache() {
        if (framebuffer) {
            esp_cache_msync(framebuffer, 1152000, ESP_CACHE_MSYNC_FLAG_DIR_C2M);   // 480 * 800 * 2bytes = 768000 or for 24bit 1152000
        }
    }

     ///* Mountet die bestehende 9MB Partition 'storage'
    bool initFS() {
        ESP_LOGI("LGFX_FS", "Mounte 9MB LittleFS Partition...");

        esp_vfs_littlefs_conf_t conf = {
            .base_path = "/littlefs",
            .partition_label = "storage", 
            .format_if_mount_failed = true,
            .dont_mount = false
        };

        esp_err_t ret = esp_vfs_littlefs_register(&conf);

        if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                ESP_LOGE("LGFX_FS", "Mount oder Format fehlgeschlagen");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE("LGFX_FS", "Partition 'storage' wurde nicht gefunden");
            }
            return false;
        }

        size_t total = 0, used = 0;
        esp_littlefs_info(conf.partition_label, &total, &used);
        ESP_LOGI("LGFX_FS", "Partition Kapazität: %d KB, Belegt: %d KB", total / 1024, used / 1024);
        return true;
    }

};