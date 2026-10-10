#pragma once
#include <Arduino.h>
struct SPIClass { bool started=false; void begin(int8_t sck=-1,int8_t miso=-1,int8_t mosi=-1,int8_t ss=-1){
  printf("[stub] SPI.begin(%d,%d,%d,%d)%s\n",sck,miso,mosi,ss,started?" (ya iniciado)":""); started=true; } };
extern SPIClass SPI;
