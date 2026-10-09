#pragma once
#include <Arduino.h>
#include <cstdarg>
#include <cstdlib>
#include "glcdfont_data.h"
// Stub con framebuffer real (solo rotación 0) y la fuente clásica de GFX.
class Adafruit_GFX {
public:
  int16_t W0=0,H0=0,_w=0,_h=0; uint8_t rot=0; uint32_t freq=32000000; bool soft=false;
  uint16_t fb[160][128]; uint32_t pixWritten=0; bool cp=false;
  void cost(uint32_t px){ pixWritten+=px; g_us += (uint32_t)((uint64_t)px*16*1000000ULL/freq) + 3; }
  void px(int x,int y,uint16_t c){ if(x>=0&&x<_w&&y>=0&&y<_h) fb[y][x]=c; }
  int16_t width(){return _w;} int16_t height(){return _h;}
  void setRotation(uint8_t r){ rot=r&3; if(rot&1){_w=H0;_h=W0;}else{_w=W0;_h=H0;} }
  void fillScreen(uint16_t c){ fillRect(0,0,_w,_h,c); }
  void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){ if(w<=0||h<=0) return; cost((uint32_t)w*h); g_us+=13; /* sobrecarga por llamada, calibrada con 7.2 */
    for(int i=x;i<x+w;i++) for(int j=y;j<y+h;j++) px(i,j,c); }
  void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c){ fillRect(x,y,1,h,c); }
  void drawRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){ fillRect(x,y,w,1,c); fillRect(x,y+h-1,w,1,c); fillRect(x,y,1,h,c); fillRect(x+w-1,y,1,h,c); }
  void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c){ fillRect(x,y,w,1,c); }
  uint8_t ts=1; int16_t cx=0,cy=0; uint16_t fg=0xFFFF,bg=0xFFFF;
  void setTextSize(uint8_t s){ts=s;} void setTextColor(uint16_t c){fg=bg=c;} void setTextColor(uint16_t c,uint16_t b){fg=c;bg=b;}
  void setCursor(int16_t x,int16_t y){cx=x;cy=y;} void setTextWrap(bool){} void cp437(bool x){cp=x;}
  void drawChar(int x,int y,unsigned char c){ if(!cp && c>=176) c++;
    for(int i=0;i<6;i++){ uint8_t line = i<5 ? gfxfont[c*5+i] : 0;
      for(int j=0;j<8;j++,line>>=1){ if(line&1) fillRect(x+i*ts,y+j*ts,ts,ts,fg); else if(bg!=fg) fillRect(x+i*ts,y+j*ts,ts,ts,bg); } } }
  size_t write(uint8_t c){ if(c=='\n'){cy+=8*ts;cx=0;return 1;} if(c=='\r') return 1; drawChar(cx,cy,c); cx+=6*ts; return 1; }
  size_t print(const char*s){ size_t n=0; while(*s){ write((uint8_t)*s++); n++; } return n; }
  size_t print(int v){ char b[16]; snprintf(b,16,"%d",v); return print(b); }
  size_t printf(const char*f,...)__attribute__((format(printf,2,3))){ char b[128]; va_list a; va_start(a,f); vsnprintf(b,128,f,a); va_end(a); return print(b); }
  uint16_t color565(uint8_t r,uint8_t g,uint8_t b){ return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3); }
  void dumpPPM(const char*name){ FILE*f=fopen(name,"wb"); fprintf(f,"P6 128 160 255\n");
    for(int y=0;y<160;y++) for(int x=0;x<128;x++){ uint16_t c=fb[y][x]; unsigned char p[3]={(unsigned char)(((c>>11)&31)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)}; fwrite(p,1,3,f);} fclose(f); }
};
