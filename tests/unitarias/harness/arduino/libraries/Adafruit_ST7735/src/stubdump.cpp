#include "Adafruit_ST7735.h"
#include <cstdlib>
extern Adafruit_ST7735 tft;
void stubDump(uint32_t ms){ static int k=0; static long at[32]; static int n=-1;
  if(n<0){ n=0; const char*t=getenv("DUMPS"); while(t&&*t){ char*e; at[n++]=strtol(t,&e,10); t=*e?e+1:e; } }
  // Carpeta de volcado: variable de entorno DUMP_DIR (por defecto, la carpeta actual)
  if(k<n && ms>=(uint32_t)at[k]){ const char*dd=getenv("DUMP_DIR"); char nm[256]; snprintf(nm,256,"%s/f_%06ld.ppm",dd?dd:".",at[k]); tft.dumpPPM(nm);
    printf("[stub] volcado %s | pixeles escritos acumulados %lu\n",nm,(unsigned long)tft.pixWritten); k++; } }
