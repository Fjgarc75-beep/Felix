#include "Arduino.h"
#include <cstdlib>
void setup(); void loop();
uint32_t g_us=0; S Serial; EspS ESP; void (*g_dumpHook)(uint32_t)=0;
const char* g_script = getenv("SCRIPT") ? getenv("SCRIPT") : "";
uint32_t g_scriptAt[16]; int g_scriptPos=0;
int g_fingerFrom=12000, g_fingerTo=21000;
bool fingerOn(uint32_t ms){ return ms>=(uint32_t)g_fingerFrom && ms<(uint32_t)g_fingerTo; }
int main(){ const char* t=getenv("TIMES"); int k=0; if(t){ char*e; const char*p=t; while(*p){ g_scriptAt[k++]=strtol(p,&e,10); p=*e?e+1:e; } }
  if(getenv("FIN")) g_fingerFrom=atoi(getenv("FIN")); if(getenv("FOUT")) g_fingerTo=atoi(getenv("FOUT"));
  uint32_t dur = getenv("DUR")? atoi(getenv("DUR")) : 32000;
  uint32_t stall = getenv("STALL")? atoi(getenv("STALL")) : 0; bool st=false;
  if(getenv("SKEW")) g_us += atoi(getenv("SKEW"));
  setup(); if(getenv("SHOWSETUP")) fprintf(stderr,"setup_end_us=%u\n",g_us); while(millis()<dur){ g_us+=500; if(stall && !st && millis()>=stall){ st=true; g_us+=500000; } loop(); if(g_dumpHook) g_dumpHook(millis()); } }
