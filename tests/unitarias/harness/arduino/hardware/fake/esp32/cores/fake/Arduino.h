#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <algorithm>
using std::min; using std::max;
typedef bool boolean; typedef uint8_t byte;
#define PI 3.1415926535897932384626433832795
#define HIGH 1
#define LOW 0
#define OUTPUT 0x03
#define INPUT_PULLDOWN 0x09
inline void pinMode(uint8_t,uint8_t){} inline int digitalRead(uint8_t){return HIGH;}
extern uint32_t g_us;
inline uint32_t millis(){return g_us/1000;} inline uint32_t micros(){return g_us;} extern void (*g_dumpHook)(uint32_t ms);
inline void delay(uint32_t ms){ if(g_dumpHook) g_dumpHook(millis()+1); g_us+=ms*1000;}
extern const char* g_script; extern uint32_t g_scriptAt[]; extern int g_scriptPos;
struct S{ size_t setTxBufferSize(size_t n){return n;} void begin(uint32_t){}
 int available(){ return (g_script[g_scriptPos] && millis()>=g_scriptAt[g_scriptPos])?1:0; }
 int read(){ if(!available()) return -1; return g_script[g_scriptPos++]; }
 void print(const char*s){fputs(s,stdout);} void println(const char*s=""){puts(s);}
 size_t printf(const char*f,...)__attribute__((format(printf,2,3))){va_list a;va_start(a,f);int r=vprintf(f,a);va_end(a);return r;}};
extern S Serial;
struct EspS { uint32_t getFreeHeap(){ return 300000; } uint32_t getMinFreeHeap(){ return 290000; } };
extern EspS ESP;
extern void (*g_dumpHook)(uint32_t ms);
