#pragma once
#include <Adafruit_GFX.h>
#include <SPI.h>
#define INITR_GREENTAB 0x00
#define INITR_BLACKTAB 0x02
#define ST77XX_BLACK 0x0000
#define ST77XX_WHITE 0xFFFF
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_BLUE 0x001F
#define ST77XX_CYAN 0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW 0xFFE0
#define ST77XX_ORANGE 0xFC00
extern void stubDump(uint32_t ms);
class Adafruit_ST7735 : public Adafruit_GFX {
public:
  Adafruit_ST7735(int8_t,int8_t,int8_t){ g_dumpHook=stubDump; }
  void initR(uint8_t v=INITR_GREENTAB){ SPI.begin(); freq=32000000; W0=128; H0=160; setRotation(0); g_us+=650000; printf("[stub] initR(0x%02X)\n",v);
    for(int y=0;y<160;y++) for(int x=0;x<128;x++) fb[y][x]=0x4208; }  // basura inicial
  void setSPISpeed(uint32_t f){ freq=80000000/((80000000+f-1)/f); }
  void invertDisplay(bool){}
};
