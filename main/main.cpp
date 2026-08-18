// Pinball Wizard 2026
// Lets gooo!!!!!

#include "LGFX_Config.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "P4_APP";
LGFX_ST7701_P4 lcd;

// IO PINS
// OLD RGB LED
#define LED_STRIP_GPIO GPIO_NUM_29
#define LED_STRIP_LED_COUNT 50



//   Physical Keys
#define        ioPinSideLeft    50     // hardware pullup 10k onboard soldered on the PCB, because its snappier then the inbuild 45k ohm, and less emf radiated
#define        ioPinSideRight   49     // hardware pullup 10k onboard soldered on the PCB
#define        ioPinFrontLeft   35     // hardware pullup 10k onboard soldered on the PCB
#define        ioPinFrontRight  34     // hardware pullup 10k onboard soldered on the PCB
#define        ioPinSideXLeft   32     // hardware pullup 10k onboard soldered on the PCB
#define        ioPinSideXRight  28     // hardware pullup 10k onboard soldered on the PCB  ACHTUNG ÄNDERN AUF >31 wegen register GPIO Matrix

#define        ioPinLedStrip    29     // 200 ohm resistor in series to the LED's data-in, VCC 5V from 5V Standby

int16_t PixelReadyToSend      =   0 ; // flag, damit am ande des main loops fest steht ob ein led frame gesendet werden kann oder nicht
// NEW RGB LED SEGMENTS
uint8_t LedSegTopFlipperLeft[10]      = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};            // intern left 1 to right 10
uint8_t LedSegTopFlipperRight[10]     = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};   // intern right to left
uint8_t LedSegFrontLeft               =  21;    // Only 1 Led, no segment  
uint8_t LedSegFrontRight              =  22;    // Only 1 Led, no segment
uint8_t LedSegFlipperBlockLeft[4]     = {23,    // Qanba 30mm Primary Flipper Switch
                                         24,    // Qanba 24mm Secondary Flipper Switch
                                         25,    // BG Light 1 Left
                                         26};   // BG Light 2 Left
uint8_t LedSegFlipperBlockRight[4]    = {27,    // Qanba 30mm Primary Flipper Switch
                                         28,    // Qanba 24mm Secondary Flipper Switch
                                         29,    // BG Light 1 Right
                                         30};   // BG Light 2 Right
uint8_t LedSegBottomLeft[1]           = {31};   // Only 1 Led, actual no segment, in future maybe more leds
uint8_t LedSegBottomRight[1]          = {32};   // Only 1 Led, actual no segment, in future maybe more leds
const uint16_t PixelCount             =  32;    // Summary of all leds, only for statistiks and the diagnostik tool


#define CANVAS_WIDTH  480
#define CANVAS_HEIGHT 800

uint8_t UIinterval       = 20;          // sets every x ms screenrefresh (20ms = 50 Hz). only touches things will trigger changes in the ui
uint8_t menuNumberActive = 0;           // Active Menu, 0 = Main Menu, 1 = Options Menu, 2 = Game Menu, etc.
uint8_t menuDrawOnce     = 1;           // Signalisiert das ein Menu einmal komplett gerendert werden muss. (wir setzen es direkt auf high, damit gleich menu0 bereit gemacht wird)
                                        // wenn >0kann sie auch als counter für die start animation genommen werden 
                                        // abhängig vom menu, danach wird sie auf 0 gesetzt um weiterzuarbeiten.
                                        // touch sollte schon während der animation abgefragt werden.
int     sleepTimer  =             15;   // 10-300 Minuten nach letztem tastendruck deep sleep shutdown. display einbrennen verhindern. akku schonen. später über filesystem oder in rtc speichern
int     ledTimeOff  =             60;   // 60 Sekunden = 1 minuten bis die leds zum stromsparen ausgehen. jede taste/touch reaktiviert timer
uint8_t menuFallback=              4;   // (einstellbar in settings menu) fallback menu, next variable defines timeout time
int     menuTimeOutToFallback =   20;   // (einstellbar in settings menu) setzt nach X sekunden menuNumberActive = menuFallback, draw once is set to high 

// Y-Koordinaten der Menu0 Sub-Sprites (fuer Rendering & Touch-Abfrage)
const uint16_t menu0_y_coords[9]     = {180, 255, 330, 405, 480, 555, 630, 705, 780};
const uint16_t menu0_sub_heights[9]  = { 75,  75,  75,  75,  75,  75,  75,  75,  20};

// Start-X-Koordinaten fuer die Slide-In Animation (TopBanner & Sub-Sprites von rechts versetzt)
const int16_t  menu0_top_banner_start_x = 480;
const int16_t  menu0_start_x[9]         = { 480 + 80, 480 + 160, 480 + 240, 480 + 320, 480 + 400, 480 + 480, 480 + 560, 480 + 640, 480 + 720 };
#define MENU0_ANIM_FRAMES 100 // 100 Frames bei 20ms Intervall (50 Hz) = 2000ms Gesamtdauer

// Vorausberechnete LUT-Tabelle (Look-Up Table): Spart jede Frame-Umrechnung zur Laufzeit komplett ein!
static int16_t menu0_anim_lut[MENU0_ANIM_FRAMES][10];

static void init_menu0_animation_lut(void) {
  for (int f = 0; f < MENU0_ANIM_FRAMES; f++) {
    float progress = (float)f / (float)(MENU0_ANIM_FRAMES - 1);

    // TopBanner X-Koordinate
    int16_t bx = (int16_t)(menu0_top_banner_start_x * (1.0f - progress));
    menu0_anim_lut[f][0] = (bx < 0) ? 0 : bx;

    // Sub-Sprites 0..8 X-Koordinaten
    for (int i = 0; i < 9; i++) {
      int16_t sx = (int16_t)(menu0_start_x[i] * (1.0f - progress));
      menu0_anim_lut[f][1 + i] = (sx < 0) ? 0 : sx;
    }
  }
}

// Persistent Sprites & Offscreen Canvas im PSRAM (bleiben im Speicher fuer schnellen Zugriff)
static lgfx::LGFX_Sprite bg_sprite;
static lgfx::LGFX_Sprite canvas;
static lgfx::LGFX_Sprite top_banner_sprites[4];
static lgfx::LGFX_Sprite menu0_sub_sprites[9];
static bool menu0_ram_loaded = false;

#include <errno.h>

// Hilfsfunktion: Lädt eine JPG-Datei aus LittleFS direkt in ein Sprite (systemschonend & sicher)
static bool load_jpg_file_to_sprite(lgfx::LGFX_Sprite &sprite, const char *path) {
  // 1. Datei aus dem LittleFS öffnen
  FILE *f = fopen(path, "rb");
  if (!f) {
    ESP_LOGE(TAG, "fopen fehlgeschlagen fuer: %s (Errno %d: %s)", path, errno, strerror(errno));
    return false;
  }

  // 2. Exacte Dateigröße der JPG-Datei ermitteln
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (sz <= 0) {
    ESP_LOGE(TAG, "Datei ist leer: %s (size=%ld)", path, sz);
    fclose(f);
    return false;
  }

  // 3. Temporären Lesepuffer im PSRAM/RAM für das JPG anfordern
  uint8_t *buf = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    buf = (uint8_t *)malloc(sz);
  }

  if (!buf) {
    ESP_LOGE(TAG, "Speicherallokierung für JPG-Puffer fehlgeschlagen: %ld Bytes", sz);
    fclose(f);
    return false;
  }

  // 4. JPG-Datei vollständig in den RAM einlesen & Datei sofort schließen
  size_t read_bytes = fread(buf, 1, sz, f);
  fclose(f);

  if (read_bytes != (size_t)sz) {
    ESP_LOGE(TAG, "fread unvollstaendig: %d von %ld Bytes gelesen", (int)read_bytes, sz);
    free(buf);
    return false;
  }

  // 5. JPG aus dem Speicherpuffer in das Sprite decodieren & Lesepuffer sofort freigeben
  bool ok = sprite.drawJpg(buf, read_bytes, 0, 0);
  free(buf);

  if (!ok) {
    ESP_LOGE(TAG, "sprite.drawJpg Decoder-Fehler fuer: %s (%ld Bytes)", path, sz);
  } else {
    ESP_LOGI(TAG, "Erfolgreich geladen: %s (%ld Bytes)", path, sz);
  }

  return ok;
}

// Ultraschneller Scanline-Compositor: Zeichnet ein 24-bit RGB888 Sprite direkt mit Hardware-Memcpy (ohne Overhead)
static inline void blit_sprite_fast(uint8_t *dst_canvas, const uint8_t *src_sprite, 
                                   int16_t x, int16_t y, int16_t sprite_w, int16_t sprite_h) {
  if (!dst_canvas || !src_sprite) return;
  if (x >= CANVAS_WIDTH || (x + sprite_w) <= 0 || y >= CANVAS_HEIGHT || (y + sprite_h) <= 0) {
    return; // Komplett ausserhalb des sichtbaren Bereichs
  }

  int16_t src_start_x = 0;
  int16_t dst_start_x = x;
  int16_t copy_w      = sprite_w;

  if (dst_start_x < 0) {
    src_start_x = -dst_start_x;
    copy_w     += dst_start_x;
    dst_start_x = 0;
  }
  if (dst_start_x + copy_w > CANVAS_WIDTH) {
    copy_w = CANVAS_WIDTH - dst_start_x;
  }
  if (copy_w <= 0) return;

  size_t copy_bytes = (size_t)copy_w * 3;
  size_t src_pitch  = (size_t)sprite_w * 3;
  size_t dst_pitch  = (size_t)CANVAS_WIDTH * 3;

  for (int16_t row = 0; row < sprite_h; row++) {
    int16_t dst_y = y + row;
    if (dst_y < 0 || dst_y >= CANVAS_HEIGHT) continue;

    const uint8_t *s = src_sprite + (row * src_pitch) + (src_start_x * 3);
    uint8_t       *d = dst_canvas + (dst_y * dst_pitch) + (dst_start_x * 3);
    memcpy(d, s, copy_bytes);
  }
}

static void load_menu0_sprites_to_ram(void) {
  if (menu0_ram_loaded) return;

  ESP_LOGI(TAG, "Lade Basis-Menu Sprites in den RAM (PSRAM)...");
  bool all_ok = true;

  // LUT-Tabelle fuer Animation einmalig initialisieren
  init_menu0_animation_lut();

  // Offscreen Canvas (480x800) im PSRAM initialisieren (Double-Buffering)
  canvas.setPsram(true);
  canvas.setColorDepth(24);
  canvas.createSprite(CANVAS_WIDTH, CANVAS_HEIGHT);

  // 4 TopBanner Sprites laden (480x180)
  for (int i = 0; i < 4; i++) {
    top_banner_sprites[i].setPsram(true);
    top_banner_sprites[i].setColorDepth(24);
    top_banner_sprites[i].createSprite(480, 180);
    char path[64];
    snprintf(path, sizeof(path), "/littlefs/TopBanner/top_menu_banner-%d-480.jpg", i + 1);
    if (!load_jpg_file_to_sprite(top_banner_sprites[i], path)) {
      all_ok = false;
    }
  }

  // 9 Sub-Sprites laden
  for (int i = 0; i < 9; i++) {
    menu0_sub_sprites[i].setPsram(true);
    menu0_sub_sprites[i].setColorDepth(24);
    menu0_sub_sprites[i].createSprite(480, menu0_sub_heights[i]);
    char path[64];
    snprintf(path, sizeof(path), "/littlefs/Sprites/menu0-sub%d.jpg", i);
    if (!load_jpg_file_to_sprite(menu0_sub_sprites[i], path)) {
      all_ok = false;
    }
  }

  if (all_ok) {
    menu0_ram_loaded = true;
    ESP_LOGI(TAG, "Alle Basis-Menu Sprites erfolgreich im RAM geladen.");
  } else {
    ESP_LOGE(TAG, "FEHLER beim Laden: Mindestens ein Sprite konnte nicht decodiert werden!");
  }
}









bool _touchDetected            = false;  
uint32_t UIintervalTimerFlag   = 0;
int _lastTouchX                = 0;
int _lastTouchY                = 0;
int _cachedTouchX              = -1;
int _cachedTouchY              = -1;
uint16_t processTouchIntervalSpeed    =  40; // touch scan speed user operates in menus via touch. 10ms-199ms timetrap für gute reactivität 10-40ms im NICHT throttle mode,
uint16_t processTouchIntervalThrottle = 500; // touch scan speed in throttle mode aka user is gaming no use of display
uint16_t processTouchNextKeyDelay     = 300; // repeat on button pressed+hold. geschwindigkeit zwischen den touch tasten ausgaben
uint32_t touchThrottleTimeout         =4000; // nach zeit x den highspeed mode scan mode verlassen und volle performance zurück in den main loop
uint16_t touchHysteresis              =   1; // Toleranz in Pixeln ([1-5] falls der Finger zittert. lustiges feature. wenn man den button reibt, gehts schneller (touchHytseresisKeyRepeatTime))
uint16_t touchHytseresisKeyRepeatTime =  80; // spielt nur im speed mode eine rolle, damit nicht zu viele touches/sec auf die moving finger methode raus gefeuert werden. es soll schneller, aber nicht max schnell sein.

uint32_t lastTouchActivityTimerFlag   =   0; // Speichert den Zeitpunkt der letzten Berührung
uint32_t processTouchNextKeyTimeFlag  =   0; 
uint32_t processTouchTimeFlag         =   0;
uint32_t timeTrapOneSecond            =   0;
int processTouchRepeatBlockerPerMenu  =   0; // kann auch mit initialisiert werden.
    

int8_t emulationMode                  =   1; // HID profiles 1 = bt Quest, 2 = btPC, 3 = bt Android , 4 = bt Iphone, 5 = bt Switch , usb-hid (per funktion und if/case rutsche) 
int                            dbglvl =   5; // Globale Debug-Variable over Serial0 UART, zentral in main.cpp
                                             // 1 only benchmark on screen, >1 to 10 goes to serial // später
                                             // über filesystem oder in rtc speichern
int dbglvlOSD                         =   5; // only a small blue translucent sprite with minimal info + benchmark
int dbglvlOSDoldState = dbglvlOSD;           // State machine for dbglvlOSD






static led_strip_handle_t led_strip = nullptr;
static uint16_t g_rainbow_hue = 0;

static void init_led_strip(void) {
  ESP_LOGI(TAG, "Initialisiere WS28xx LED-Strip an GPIO %d (%d LEDs)...", 
           (int)LED_STRIP_GPIO, LED_STRIP_LED_COUNT);
  led_strip_config_t strip_config = {};
  strip_config.strip_gpio_num = LED_STRIP_GPIO;
  strip_config.max_leds = LED_STRIP_LED_COUNT;
  strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
  strip_config.led_model = LED_MODEL_WS2812;
  strip_config.flags.invert_out = false;

  led_strip_rmt_config_t rmt_config = {};
  rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
  rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz
  rmt_config.mem_block_symbols = 64;
  rmt_config.flags.with_dma = false;

  esp_err_t err =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
  if (err == ESP_OK) {
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "LED-Strip erfolgreich initialisiert.");
  } else {
    ESP_LOGE(TAG, "LED-Strip Initialisierung fehlgeschlagen! Error: %s",
             esp_err_to_name(err));
  }
}

// Hilfsfunktion: HSV zu RGB Konvertierung
static void hsv_to_rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r,
                       uint32_t *g, uint32_t *b) {
  h %= 360;
  uint32_t rgb_max = v * 255 / 255;
  uint32_t rgb_min = rgb_max * (255 - s) / 255;
  uint32_t i = h / 60;
  uint32_t diff = h % 60;
  uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

  switch (i) {
  case 0:
    *r = rgb_max;
    *g = rgb_min + rgb_adj;
    *b = rgb_min;
    break;
  case 1:
    *r = rgb_max - rgb_adj;
    *g = rgb_max;
    *b = rgb_min;
    break;
  case 2:
    *r = rgb_min;
    *g = rgb_max;
    *b = rgb_min + rgb_adj;
    break;
  case 3:
    *r = rgb_min;
    *g = rgb_max - rgb_adj;
    *b = rgb_max;
    break;
  case 4:
    *r = rgb_min + rgb_adj;
    *g = rgb_min;
    *b = rgb_max;
    break;
  default:
    *r = rgb_max;
    *g = rgb_min;
    *b = rgb_max - rgb_adj;
    break;
  }
}

// Einen Frame des Regenbogens fortschreiten lassen
static void step_rainbow(void) {
  if (!led_strip)
    return;
  for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
    uint32_t hue = (g_rainbow_hue + (i * 360 / LED_STRIP_LED_COUNT)) % 360;
    uint32_t r = 0, g = 0, b = 0;
    hsv_to_rgb(hue, 255, 100, &r, &g, &b);
    led_strip_set_pixel(led_strip, i, r, g, b);
  }
  led_strip_refresh(led_strip);
  g_rainbow_hue = (g_rainbow_hue + 3) % 360;
}

// Regenbogeneffekt fuer eine bestimmte Dauer (in ms)
void run_rainbow(uint32_t duration_ms) {
  TickType_t start_tick = xTaskGetTickCount();
  TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);

  while ((xTaskGetTickCount() - start_tick) < duration_ticks) {
    step_rainbow();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Block-Definitionen fuer Fisher-Yates Effect
#define BLOCK_SIZE 8
#define CANVAS_WIDTH 480
#define CANVAS_HEIGHT 800
#define GRID_COLS (CANVAS_WIDTH / BLOCK_SIZE)  // 60 Spalten
#define GRID_ROWS (CANVAS_HEIGHT / BLOCK_SIZE) // 100 Zeilen
#define TOTAL_BLOCKS (GRID_COLS * GRID_ROWS)   // 6000 Bloecke

static void init_and_shuffle_blocks(uint16_t *indices, size_t count) {
  for (size_t i = 0; i < count; i++) {
    indices[i] = i;
  }
  // Fisher-Yates Shuffle
  for (size_t i = count - 1; i > 0; i--) {
    size_t j = esp_random() % (i + 1);
    std::swap(indices[i], indices[j]);
  }
}

extern "C" void app_main(void) {
  // LED Strip auf GPIO 29 initialisieren
  init_led_strip();

  if (!lcd.initFS()) {
    ESP_LOGE(TAG, "LittleFS konnte nicht initialisiert werden!");
  } else {
    ESP_LOGI(TAG, "LittleFS erfolgreich gemountet.");
  }

  if (!lcd.initBus()) {
    ESP_LOGE(TAG, "Display Init fehlgeschlagen!");
    return;
  }

  lcd.init();
  lcd.setBrightness(200); // set display brightness
                          // vTaskDelay(pdMS_TO_TICKS(500));

  // 1. Display zum Start mit Schwarz leeren
  lcd.sprite.fillSprite(0x000000);
  lcd.pushCache();

  // 2. Offscreen-Sprite fuer Hintergrundbild (PSRAM) & Shuffle-Array (Heap) allozieren
  bg_sprite.setPsram(true);
  bg_sprite.setColorDepth(24);
  bg_sprite.createSprite(CANVAS_WIDTH, CANVAS_HEIGHT);

  uint16_t *block_indices = (uint16_t *)heap_caps_malloc(
      TOTAL_BLOCKS * sizeof(uint16_t), MALLOC_CAP_8BIT);

  if (!bg_sprite.getBuffer() || !block_indices) {
    ESP_LOGE(TAG, "Speicherallokierung fuer Offscreen-Puffer/Block-Indices "
                  "fehlgeschlagen!");
    if (block_indices)
      free(block_indices);
    bg_sprite.deleteSprite();
    vTaskDelay(pdMS_TO_TICKS(1000));
    return;
  }

  ESP_LOGI(TAG, "Lade Bild PBWZ800x480.jpg in Offscreen-Puffer...");
  bool ok = load_jpg_file_to_sprite(bg_sprite, "/littlefs/bg/PBWZ800x480.jpg");
  if (!ok) {
    ESP_LOGE(TAG, "Bild konnte nicht geladen werden!");
    bg_sprite.fillSprite(0x0000FF); // Blau als Fallback
    bg_sprite.setTextColor(0xFFFF);
    bg_sprite.drawString("Bild-Fehler!", 10, 10);
  }

  // 3. Fisher-Yates Shuffle der 8x8 Bloecke
  init_and_shuffle_blocks(block_indices, TOTAL_BLOCKS);

  uint8_t *src_buf = (uint8_t *)bg_sprite.getBuffer();
  uint8_t *dst_buf = (uint8_t *)lcd.sprite.getBuffer();

  // 4. Geordneter Zufall (abgeschlossen in 3000ms: 20 Bloecke pro Frame bei
  // 10ms Delay -> 300 Frames = 3.0s)
  for (int i = 0; i < TOTAL_BLOCKS; i += 20) {
    for (int b = 0; b < 20 && (i + b) < TOTAL_BLOCKS; b++) {
      uint16_t idx = block_indices[i + b];
      int bx = (idx % GRID_COLS) * BLOCK_SIZE;
      int by = (idx / GRID_COLS) * BLOCK_SIZE;

      for (int ry = 0; ry < BLOCK_SIZE; ry++) {
        uint32_t offset = ((by + ry) * CANVAS_WIDTH + bx) * 3;
        memcpy(&dst_buf[offset], &src_buf[offset], BLOCK_SIZE * 3);
      }
    }

    lcd.pushCache();
    step_rainbow();
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(TAG, "Bild vollstaendig per Fisher-Yates aufgebaut.");

  // 5. Shuffle-Array (Heap) freigeben (bg_sprite bleibt im PSRAM erhalten!)
  free(block_indices);

  // 6. 3 Sekunden Regenbogen anzeigen
  run_rainbow(3000);

  // end intro

  // Basis-Menu Sprites im RAM bereitstellen (laden nur einmalig)
  load_menu0_sprites_to_ram();

  int banner_idx = esp_random() % 4;
  int anim_frame = 0;
  UIintervalTimerFlag = (uint32_t)(esp_timer_get_time() / 1000);

  while (1) {
    uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000);

    // Timetrap fuer Bildschirmaufbau / Animation alle UIinterval (40ms)
    if ((current_time - UIintervalTimerFlag) >= UIinterval) {
      UIintervalTimerFlag = current_time;

      if (anim_frame < MENU0_ANIM_FRAMES) {
        uint8_t *canvas_buf = (uint8_t *)canvas.getBuffer();
        const uint8_t *bg_buf = (const uint8_t *)bg_sprite.getBuffer();

        // 1. Hintergrundbild in 1 Direct-Block in den Canvas kopieren (< 0.05ms)
        memcpy(canvas_buf, bg_buf, CANVAS_WIDTH * CANVAS_HEIGHT * 3);

        // 2. TopBanner mit vorausberechneter X-Koordinate aus LUT blitten (Zero-Math)
        int16_t banner_cur_x = menu0_anim_lut[anim_frame][0];
        blit_sprite_fast(canvas_buf, (const uint8_t *)top_banner_sprites[banner_idx].getBuffer(),
                         banner_cur_x, 0, 480, 180);

        // 3. Sub-Sprites mit vorausberechneten X-Koordinaten aus LUT blitten (Zero-Math)
        for (int i = 0; i < 9; i++) {
          int16_t cur_x = menu0_anim_lut[anim_frame][1 + i];
          blit_sprite_fast(canvas_buf, (const uint8_t *)menu0_sub_sprites[i].getBuffer(),
                           cur_x, menu0_y_coords[i], 480, menu0_sub_heights[i]);
        }

        // 4. Fertigen Frame in einem Rutsch per DMA uebertragen (100% Tear-Free Double Buffer)
        memcpy(lcd.framebuffer, canvas_buf, CANVAS_WIDTH * CANVAS_HEIGHT * 3);
        lcd.pushCache();

        anim_frame++;
      } else {
        // Animation abgeschlossen: Alle Sprites stehen stabil auf x=0
        // 2000ms Wartezeit am Ende der Animation
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Bildschirm schwarz loeschen
        lcd.sprite.fillSprite(0x000000);
        lcd.pushCache();

        // Neuer Zufalls-TopBanner & Animation fuer die naechste Runde zuruecksetzen
        banner_idx = esp_random() % 4;
        anim_frame = 0;
        UIintervalTimerFlag = (uint32_t)(esp_timer_get_time() / 1000);
      }
    }

    // Kurzer Task-Yield für FreeRTOS IDLE-Task & Watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}