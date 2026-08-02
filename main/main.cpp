#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "LGFX_Config.hpp"
#include "esp_random.h"


static const char *TAG = "P4_APP";
LGFX_ST7701_P4 lcd;


/// c64 hilfsfunktion für text infos
void showC64Intro(const char* title, const char* info) {
    // C64 Farben: Blau (#4040E0), Hellblau (#A0A0FF)
    uint32_t c64_blue = lgfx::color888(64, 64, 224);
    uint32_t c64_light = lgfx::color888(160, 160, 255);

    lcd.sprite.fillSprite(c64_blue);
    lcd.sprite.setTextColor(c64_light);
    lcd.sprite.setTextSize(2);
    lcd.sprite.setFont(&fonts::FreeSansBold9pt7b); // Kompakterer Font für 10 Zeilen
    
    lcd.sprite.setCursor(20, 100);
    lcd.sprite.println("**** COMMODORE ****");
    lcd.sprite.setCursor(20, 130);
    lcd.sprite.println("*** 64 BASIC V2 ***");
    lcd.sprite.setCursor(2, 200);
    lcd.sprite.print("DEMO: "); 
    lcd.sprite.println(title);
    lcd.sprite.println("");
    
    // Die Info-Texte (maximal 10 Zeilen/Wörter)
    lcd.sprite.setCursor(2, 300);
    lcd.sprite.println(info);
    
    lcd.sprite.println("");
    lcd.sprite.println("LOADING...");
    lcd.sprite.print("_"); // Der blinkende Cursor-Ersatz

    lcd.pushCache();
    vTaskDelay(pdMS_TO_TICKS(2000)); // 1 Sekunde Anzeigezeit
}




extern "C" void app_main(void)
{
    // Hier muss der Aufruf rein!
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
    lcd.setBrightness(200);                    // set display brightness
    vTaskDelay(pdMS_TO_TICKS(500));

/// Einmaliges Setup Sprite 2 foreground debug window (global oder in app_main)  DEBUG OVERLAY
lgfx::LGFX_Sprite overlay(&lcd.sprite); 
overlay.setPsram(true); // Nutzt den 32MB Speicher des P4
overlay.setColorDepth(24); // 24-Bit für konsistente Farben
overlay.createSprite(380, 100); // Größe des Kastens
overlay.setPivot(0, 0);

/// einmalig zweiten frame buffer reservieren, um tearing also den realtime bildaufbau zu vermeiden.
// In der main.cpp vor der Schleife:
// 480 * 800 * 3 Bytes = 1.152.000 Bytes
uint8_t* back_buffer = (uint8_t*)heap_caps_malloc(1152000, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);








///main()
    showC64Intro("ESP32-P4", "SINGLE CORE\ngfx tests\nwith LovyanGFX\nlovyanGFX");
    showC64Intro("ESP32-P4", "with 2 cores\ntheoreticly\neverything\ncould run at\ndouble speed!");
    ESP_LOGI(TAG, "Starte Animation mit jpg, kreisen, Rechtecken, Text und Sprite...");
    // --- DEMO 1: MANDELBROT ---
    showC64Intro("P4 DEMO", "JPG\n24BIT\nRGB888\nand text in\nSprite 2\nlovyanGFX");
    while (1) {
        
        // --- TEIL 0: Bild von LittleFS laden ---
        // Wir nutzen den Pfad "/littlefs/", da du ihn in initFS so definiert hast.
        ESP_LOGI(TAG, "Lade Bild PBWZ800x480.jpg...");
        
        // drawJpgFile lädt direkt in das Sprite (unseren Canvas)
        // Falls das Bild 800x480 ist, das Display aber 480x800, wird es oben links gezeichnet.
        bool ok = lcd.sprite.drawJpgFile("/littlefs/PBWZ800x480.jpg", 0, 0);
        
        if (!ok) {
            ESP_LOGE(TAG, "Bild konnte nicht geladen werden! Datei vorhanden?");
            lcd.sprite.setTextColor(0xFFFF);
            lcd.sprite.drawString("Bild-Fehler!", 10, 10);
        }

        lcd.pushCache();
        vTaskDelay(pdMS_TO_TICKS(3000));

/// Zeichne einen Kasten, der mit dem Hintergrund "verschmilzt"
    lcd.sprite.startWrite();

        // 128 steht hier für ca. 50% Transparenz (0-255)
        // befülle zweites sprite  
    overlay.fillSprite(lgfx::color888(10, 10, 200)); // Blau füllen
        // overlay.fillRect(50, 400, 380, 100, lgfx::color888(0, 0, 255));
        //overlay.pushSprite(&lcd.sprite, 50, 400, 40); // Mit Alpha 128 (50%) auf das Haupt-DSI-Sprite schieben
    overlay.pushSprite(&lcd.sprite, 50, 300, 128);
    lcd.pushCache();

lcd.sprite.setTextColor(lgfx::color888(255, 255, 255));
lcd.sprite.setTextSize(3); // Große Schrift
lcd.sprite.drawString("SPRITE", 100, 330);
lcd.sprite.endWrite();
lcd.pushCache();
vTaskDelay(pdMS_TO_TICKS(3000));

showC64Intro("GEOMETRIC", "10000\nCircles\nand\nsquares\nlovyanGFX");
        // --- TEIL 1: Zufällige Geometrie ---
        lcd.sprite.startWrite();
        for (int i = 0; i < 10000; i++) {
            int x = rand() % 480;
            int y = rand() % 800;
            int w = (rand() % 100) + 2;
            int h = (rand() % 100) + 2;
            int r = (rand() % 50)  + 2;
            uint32_t color = (uint32_t)rand()% 16777216;

            if (i % 2 == 0) {
                lcd.sprite.fillCircle(x, y, r, color);
            } else {
                lcd.sprite.fillRect(x, y, w, h, color);
            }
        }
        lcd.sprite.endWrite();
        lcd.pushCache();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // --- TEIL 2: Vier senkrechte Farbbalken (R, G, B, W) ---
        // Breite pro Balken: 480 / 4 = 120 Pixel

        lcd.sprite.startWrite();

        // Wir nutzen kein Hex mehr, sondern die Funktion, die beim Text funktionierte
        lcd.sprite.fillRect(0,   0, 120, 800, lgfx::color888(255, 0, 0));     // Rot
        lcd.sprite.fillRect(120, 0, 120, 800, lgfx::color888(0, 255, 0));     // Grün
        lcd.sprite.fillRect(240, 0, 120, 800, lgfx::color888(0, 0, 255));     // Blau
        lcd.sprite.fillRect(360, 0, 120, 800, lgfx::color888(255, 255, 255)); // Weiß
        
        lcd.sprite.endWrite();
        lcd.pushCache();
        
        ESP_LOGI(TAG, "Farbbalken gezeichnet.");
        vTaskDelay(pdMS_TO_TICKS(1000));

        lcd.sprite.startWrite();
        lcd.sprite.fillSprite(0xFFFFFF);          // Hintergrund Weiß
        
        // Schrift-Setup
        lcd.sprite.setTextSize(4); // Große Schrift
        lcd.sprite.setFont(&fonts::FreeSansBold18pt7b); // Schöner großer Font
        
        // Test 1: Rot
        lcd.sprite.setTextColor(lgfx::color888(255, 0, 0)); 
        lcd.sprite.drawString("RED", 1, 100);
        
        // Test 2: Grün
        lcd.sprite.setTextColor(lgfx::color888(0, 255, 0));
        lcd.sprite.drawString("GREEN", 1, 200);
        
        // Test 3: Blau
        lcd.sprite.setTextColor(lgfx::color888(0, 0, 255));
        lcd.sprite.drawString("BLUE", 1, 300);
        
        // Test 4: Schwarz
        lcd.sprite.setTextColor(lgfx::color888(0, 0, 0));
        lcd.sprite.drawString("BLACK", 1, 400);

        lcd.sprite.endWrite();
        lcd.pushCache();
        
        ESP_LOGI(TAG, "Text-Test auf dem Schirm");
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Sekunde(n) warten

// --- TEIL 4: Das "Gute Nacht" Apfelmännchen (Single Core) ---
showC64Intro("FRACTAL", "realtime\ncalculating\nNO doublebuffer\ndirectly into\nframebuffer\n(DMA PSRAM)");
for(uint8_t count = 0; count < 3; count++){
        ESP_LOGI(TAG, "Rendere Fraktal...");
        
        // Zufallswerte für die Abwechslung
        float zoom = 0.002f + ((float)(esp_random() % 100) / 20000.0f);
        float moveX = ((float)(esp_random() % 2000) / 1000.0f) - 1.5f;
        float moveY = ((float)(esp_random() % 2000) / 1000.0f) - 1.0f;
        uint32_t colorSalt = esp_random();
        
        uint8_t* buf = (uint8_t*)lcd.framebuffer;
        const int max_iter = 255; // Niedriger für mehr Speed im Single-Core

        for (int y = 0; y < 800; y++) {
            for (int x = 0; x < 480; x++) {
                // Berechnung
                float pr = (x - 240) * zoom + moveX;
                float pi = (y - 400) * zoom + moveY;
                float zr = 0, zi = 0;
                int iter = 0;
                
                while (zr*zr + zi*zi < 4 && iter < max_iter) {
                    float temp = zr*zr - zi*zi + pr;
                    zi = 2*zr*zi + pi;
                    zr = temp;
                    iter++;
                }

                // Pixel direkt in den 24-Bit Buffer schreiben (RGB888)
                int idx = (y * 480 + x) * 3;
                if (iter == max_iter) {
                    buf[idx] = 0; buf[idx+1] = 0; buf[idx+2] = 0; // Schwarz
                } else {
                    // Einfaches Farbschema mit Salt
                    buf[idx]   = (iter * 5 + colorSalt) % 256;         // R
                    buf[idx+1] = (iter * 8 + (colorSalt >> 8)) % 256;  // G
                    buf[idx+2] = (iter * 11 + (colorSalt >> 16)) % 256; // B
                }
            }
        }

        lcd.pushCache(); // Wichtig, damit der P4 den Speicher leert
        ESP_LOGI(TAG, "Fraktal fertig.");
        vTaskDelay(pdMS_TO_TICKS(200));
    }
vTaskDelay(pdMS_TO_TICKS(800));




// --- TEIL 5: Die Quadrat-Matrix (Sierpinski Engine) ---
showC64Intro("Sierpinski", "realtime fractal\ncalculating\nNO doublebuffer\ndirect into fb");
for(uint8_t count = 0; count < 5; count++){
ESP_LOGI(TAG, "Starte Lineare Fraktal-Engine...");

uint8_t* buf = (uint8_t*)lcd.framebuffer;
uint32_t salt = esp_random();

// Helfer-Funktion zum Zeichnen eines 24-Bit Pixels (Direkt im RAM)
auto drawPixel24 = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < 480 && y >= 0 && y < 800) {
        int idx = (y * 480 + x) * 3;
        buf[idx] = r; buf[idx+1] = g; buf[idx+2] = b;
    }
};

// Helfer-Funktion für ein gefülltes Rechteck (Direkt im RAM)
auto fillRect24 = [&](int x, int y, int w, int h, uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            drawPixel24(j, i, r, g, b);
        }
    }
};

// Die rekursive Engine
std::function<void(int, int, int, int)> drawCarpet = 
    [&](int x, int y, int size, int depth) {
    if (depth == 0) return;

    int newSize = size / 3;
    
    // Das mittlere Quadrat ausschneiden/färben
    // Wir nutzen das Salt für neon-artige Farben
    uint32_t color = ((salt ^ (depth * 0x123456)) & 0xFFFFFF);
    fillRect24(x + newSize, y + newSize, newSize, newSize, color);

    // Rekursion für die 8 umliegenden Quadrate
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (i == 1 && j == 1) continue; // Mitte überspringen
            drawCarpet(x + i * newSize, y + j * newSize, newSize, depth - 1);
        }
    }
};

// Screen leeren
memset(buf, 0, 480 * 800 * 3);

// Starte das Fraktal (zentriert auf dem 480x800 Schirm)
int carpetSize = 450; 
int startX = (480 - carpetSize) / 2;
int startY = (800 - carpetSize) / 2;
int maxDepth = 4 + (esp_random() % 2); // Variierende Tiefe 4-5

drawCarpet(startX, startY, carpetSize, maxDepth);

lcd.pushCache();
ESP_LOGI(TAG, "Sierpinski-Teppich fertig gerendert.");
vTaskDelay(pdMS_TO_TICKS(1000));
}

/// sierpinski animation NOT BUFFERED. Directly write into DMA PSRAM frame buffer. shows tearing. 
showC64Intro("Sierpinski", "realtime fractal\nNO doublebuffer\ndirekt into\n dma framebuffer");
// --- TEIL 6: Die Sierpinski-Loop-Engine (Direktzugriff) ---
ESP_LOGI(TAG, "Starte Sierpinski-Loop...");

uint8_t* buf = (uint8_t*)lcd.framebuffer;
uint8_t r_base = 0, g_base = 255, b_base = 200; // Dein Neon-Cyan

for (int frame = 0; frame < 100; frame=frame +4) {
    // 1. Framebuffer leeren
    memset(buf, 0, 1152000); 

    // 2. Zoom berechnen
    float zoom = powf(3.0f, (float)frame / 300.0f);
    float currentSize = 480.0f * zoom;
    float startX = 240.0f - (currentSize / 2.0f);
    float startY = 400.0f - (currentSize / 2.0f);

    // 3. Rekursive Zeichen-Funktion (direkt integriert)
    std::function<void(float, float, float, int)> drawLoop = 
        [&](float x, float y, float size, int depth) {
        if (depth == 0 || size < 1.0f) return;

        float newSize = size / 3.0f;
        
        // Sichtbarkeits-Check (Clipping)
        if (x + size > 0 && x < 480 && y + size > 0 && y < 800) {
            
            // --- ERSATZ FÜR fastRect: Direktes Füllen des Speichers ---
            int rx = (int)(x + newSize);
            int ry = (int)(y + newSize);
            int rw = (int)newSize;
            int rh = (int)newSize;
            
            uint8_t r = r_base >> (6 - depth);
            uint8_t g = g_base >> (6 - depth);
            uint8_t b = b_base >> (6 - depth);

            for (int i = ry; i < ry + rh; i++) {
                if (i < 0 || i >= 800) continue;
                int rowOffset = i * 480 * 3;
                for (int j = rx; j < rx + rw; j++) {
                    if (j < 0 || j >= 480) continue;
                    int idx = rowOffset + (j * 3);
                    buf[idx] = r;     // R
                    buf[idx + 1] = g; // G
                    buf[idx + 2] = b; // B
                }
            }
            // -------------------------------------------------------

            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    if (i == 1 && j == 1) continue; 
                    drawLoop(x + i * newSize, y + j * newSize, newSize, depth - 1);
                }
            }
        }
    };

    drawLoop(startX, startY, currentSize, 6);

    // 4. Cache-Sync und MIPI-Update
    lcd.pushCache();
    vTaskDelay(pdMS_TO_TICKS(16)); // ~60 FPS Ziel
}

/// double buffer test
// --- TEIL 7: Sierpinski-Loop mit Double-Buffering ---
showC64Intro("Sierpinski", "realtime fractal\nWITH\ndoublebuffer\n");
ESP_LOGI(TAG, "Starte Jitter-freie Animation...");

uint8_t* display_fb = (uint8_t*)lcd.framebuffer;
// uint8_t r_base = 0, g_base = 255, b_base = 200;

for (int frame = 0; frame < 200; frame = frame +4) {
    // 1. Wir zeichnen ALLES in den back_buffer (unsichtbar)
    memset(back_buffer, 0, 1152000); 

    float zoom = powf(3.0f, (float)frame / 300.0f);
    float currentSize = 480.0f * zoom;
    float startX = 240.0f - (currentSize / 2.0f);
    float startY = 400.0f - (currentSize / 2.0f);

    std::function<void(float, float, float, int)> drawLoop = 
        [&](float x, float y, float size, int depth) {
        if (depth == 0 || size < 1.0f) return;
        float newSize = size / 3.0f;
        
        if (x + size > 0 && x < 480 && y + size > 0 && y < 800) {
            int rx = (int)(x + newSize);
            int ry = (int)(y + newSize);
            int rw = (int)newSize;
            int rh = (int)newSize;
            
            uint8_t r = r_base >> (6 - depth);
            uint8_t g = g_base >> (6 - depth);
            uint8_t b = b_base >> (6 - depth);

            for (int i = ry; i < ry + rh; i++) {
                if (i < 0 || i >= 800) continue;
                int rowOffset = i * 480 * 3;
                for (int j = rx; j < rx + rw; j++) {
                    if (j < 0 || j >= 480) continue;
                    int idx = rowOffset + (j * 3);
                    // Wir schreiben in den BACK_BUFFER!
                    back_buffer[idx] = r;
                    back_buffer[idx + 1] = g;
                    back_buffer[idx + 2] = b;
                }
            }

            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    if (i == 1 && j == 1) continue; 
                    drawLoop(x + i * newSize, y + j * newSize, newSize, depth - 1);
                }
            }
        }
    };

    drawLoop(startX, startY, currentSize, 6);

    // 2. JETZT kopieren wir den fertigen Frame blitzschnell in den Display-FB
    // Der P4 nutzt hier intern Optimierungen für PSRAM-Kopien
    memcpy(display_fb, back_buffer, 1152000);

    // 3. Cache synchronisieren
    lcd.pushCache();
    
    vTaskDelay(pdMS_TO_TICKS(16)); 
}


// --- TEIL 8: Zeitgesteuerter Amiga Boing Ball ---
ESP_LOGI(TAG, "Starte Amiga Ball Demo...");
showC64Intro("AMIGA BALL", "realtime\nWITH\ndoublebuffer\n");

// uint8_t* display_fb = (uint8_t*)lcd.framebuffer;
float angle = 0;
float ballX = 240, ballY = 400;
float velX = 15.6f, velY =11.4f; // Etwas flotter für 2 Sekunden

// Startzeit in Mikrosekunden holen
int64_t startTime = esp_timer_get_time();

// Läuft exakt 2.000.000 Mikrosekunden
while ((esp_timer_get_time() - startTime) < 8000000) {
    memset(back_buffer, 0, 1152000);

    // Physik
    ballX += velX;
    ballY += velY;
    if (ballX < 60 || ballX > 420) velX = -velX;
    if (ballY < 60 || ballY > 740) velY = -velY;

    angle += 0.2f; 
    const int radius = 60;

    for (int py = -radius; py < radius; py++) {
        float dx = sqrtf(radius * radius - py * py);
        int screenY = (int)(ballY + py);
        if (screenY < 0 || screenY >= 800) continue;

        int rowOffset = screenY * 480 * 3;

        for (int px = (int)-dx; px < (int)dx; px++) {
            int screenX = (int)(ballX + px);
            if (screenX < 0 || screenX >= 480) continue;

            // Die Projektion
            float longitude = asinf(px / dx) + angle;
            float latitude = acosf(py / (float)radius);
            
            // Checkerboard
            bool check = ((int)(longitude * 4 / M_PI) % 2 == 0) ^ ((int)(latitude * 8 / M_PI) % 2 == 0);
            
            int idx = rowOffset + (screenX * 3);
            if (check) {
                back_buffer[idx] = 255; back_buffer[idx+1] = 0; back_buffer[idx+2] = 0;
            } else {
                back_buffer[idx] = 255; back_buffer[idx+1] = 255; back_buffer[idx+2] = 255;
            }
        }
    }

    memcpy(display_fb, back_buffer, 1152000);
    lcd.pushCache();
    
    // Kurze Pause um die CPU nicht zu 100% zu grillen
    vTaskDelay(pdMS_TO_TICKS(8)); 
}

ESP_LOGI(TAG, "Boing Ball Demo beendet.");


/// next effekt
// --- TEIL 10: Der organische Plasma-Wormhole ---
showC64Intro("PLASMA", "Realtime calculated\nWORMHOLE\nAlpha Blending\nWITH\ndoublebuffer\n");
ESP_LOGI(TAG, "Starte Wormhole-Animation...");

// uint8_t* display_fb = (uint8_t*)lcd.framebuffer;
int64_t wormStart = esp_timer_get_time();
float elapsed = 0;

while ((esp_timer_get_time() - wormStart) < 20000000) {
    elapsed += 0.04f;

    // Driften des Mittelpunkts (Lissajous-Kurve)
    float centerX = 240.0f + sinf(elapsed * 0.7f) * 80.0f;
    float centerY = 400.0f + cosf(elapsed * 0.5f) * 120.0f;

    // Wir nutzen den back_buffer für das aktuelle Rendering
    for (int y = 0; y < 800; y += 2) { 
        float dy = (y - centerY);
        int rowIdx = y * 480 * 3;

        for (int x = 0; x < 480; x += 2) {
            float dx = (x - centerX);

            // Polar-Koordinaten berechnen
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < 1.0f) dist = 1.0f;
            float angle = atan2f(dy, dx);

            // Tunnel-Textur-Logik (Z-Tiefe + Rotation)
            // Die Tiefe wird durch 1/distanz erzeugt
            float z = (4000.0f / dist) + (elapsed * 30.0f);
            float a = (angle * 3.0f / M_PI) + (elapsed * 0.5f);

            // Erzeuge ein Plasma-Muster basierend auf Z und A
            uint8_t pattern = (uint8_t)((sinf(z * 0.2f) + sinf(a * 5.0f)) * 127 + 128);
            
            // "Neblige" Farbmischung (Neon-Blue & Purple)
            uint8_t r = pattern / 4;
            uint8_t g = pattern / 8;
            uint8_t b = pattern;

            // Zeichne 2x2 Pixel-Blöcke
            for (int yy = 0; yy < 2; yy++) {
                int py = y + yy;
                if (py >= 800) continue;
                for (int xx = 0; xx < 2; xx++) {
                    int px = x + xx;
                    if (px >= 480) continue;
                    
                    int idx = (py * 480 + px) * 3;

                    // --- HALBTRANSPARENZ (Alpha Blending) ---
                    // Wir mischen den neuen Pixel (50%) mit dem alten (50%)
                    back_buffer[idx]   = (back_buffer[idx]   + r) >> 1;
                    back_buffer[idx+1] = (back_buffer[idx+1] + g) >> 1;
                    back_buffer[idx+2] = (back_buffer[idx+2] + b) >> 1;
                }
            }
        }
    }

    // Den fertigen Frame in den echten Framebuffer schieben
    memcpy(display_fb, back_buffer, 1152000);
    lcd.pushCache();
    vTaskDelay(pdMS_TO_TICKS(5)); 
}


}

}