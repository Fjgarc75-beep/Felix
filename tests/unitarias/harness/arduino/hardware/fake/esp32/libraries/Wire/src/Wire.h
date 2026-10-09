#pragma once
#include "Arduino.h"
#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 128
#endif
extern bool fingerOn(uint32_t ms);
extern int g_ledWritesBeforeSetup;
class TwoWire { public:
 uint8_t regs[256]={0}; uint8_t addr=0, ptr=0; int nbytes=0; uint8_t rx[256]; int rxn=0, rxi=0; uint32_t clk=0;
 uint64_t consumed=0, producedBase=0; uint64_t lost=0;
 TwoWire(){regs[0xFF]=0x15; regs[0xFE]=0x03;}
 static double sfs(){ static double f = getenv("SFS")? atof(getenv("SFS")) : 100.0; return f; }
 uint64_t produced(){ return (regs[0x09]&7)==3 ? (uint64_t)(g_us*sfs()/1e6) - producedBase : 0; }
 void por(){ static long at=getenv("POR_AT")?atol(getenv("POR_AT")):-1; static bool done=false; if(!done && at>=0 && (long)millis()>=at){ done=true; for(int r=0;r<0xFE;r++) regs[r]=0; consumed=0; producedBase=(uint64_t)(g_us*sfs()/1e6); } }
 void sync(){ por(); uint64_t p=produced(); if(p-consumed>32){ uint64_t ex=p-consumed-32; lost+=ex; consumed+=ex; regs[0x05]=(uint8_t)std::min<uint64_t>(31,ex+regs[0x05]); } regs[0x04]=p%32; regs[0x06]=consumed%32; }
 bool begin(){return true;} bool begin(int,int,uint32_t f=0){clk=f?f:100000;return true;}
 bool end(){return true;} bool setClock(uint32_t f){clk=f;return true;} uint32_t getClock(){return clk;} void setTimeOut(uint16_t){}
 void beginTransmission(uint8_t a){addr=a;nbytes=0;}
 size_t write(uint8_t v){ if(nbytes==0) ptr=v; else { if(ptr==0x09 && (v&0x40)) { v&=~0x40; for(int r=0;r<0xFE;r++) regs[r]=0; }
   if(ptr==0x09 && (v&7)==3 && (regs[0x09]&7)!=3) producedBase=(uint64_t)(g_us*sfs()/1e6);
   if(ptr==0x04 && v==0){ consumed=produced(); regs[0x05]=0; }
   regs[ptr]=v; ptr++; } nbytes++; return 1;}
 static bool dead(){ static long f=getenv("DEAD_FROM")?atol(getenv("DEAD_FROM")):-1; static long t=getenv("DEAD_TO")?atol(getenv("DEAD_TO")):-1; long m=(long)millis(); return f>=0 && m>=f && (t<0||m<t); }
 uint8_t endTransmission(bool=true){ if(dead()) return 2; return addr==0x57?0:2; }
 uint32_t sampleVal(uint64_t idx, bool ir){
   double tsec = (double)(idx+producedBase)/sfs(); uint32_t ms=(uint32_t)(tsec*1000.0);
   static double ramp = getenv("RAMP_MS")? atof(getenv("RAMP_MS")) : 0;
   static double hover = getenv("HOVER")? atof(getenv("HOVER")) : 0;
   static double press = getenv("PRESS")? atof(getenv("PRESS")) : 0;
   static double bpm = getenv("BPM")? atof(getenv("BPM")) : 72;
   static double hrv = getenv("HRV")? atof(getenv("HRV")) : 0.03;      // modulación respiratoria del ritmo
   static double dic = getenv("DICROTIC")? atof(getenv("DICROTIC")) : 0.45;
   static double nz  = getenv("NOISE")? atof(getenv("NOISE")) : 100;
   extern int g_fingerFrom, g_fingerTo;
   double fr;
   if (ms < (uint32_t)g_fingerFrom) fr = 0; else if (ms < (uint32_t)g_fingerTo) fr = ramp>0 ? std::min(1.0,(ms-g_fingerFrom)/ramp) : 1;
   else fr = ramp>0 ? std::max(0.0,1.0-(ms-g_fingerTo)/ramp) : 0;
   double noise=((rand()%2001)-1000)/1000.0*nz; double led=regs[ir?0x0D:0x0C]/31.0;
   static double amp = getenv("PULSE")? atof(getenv("PULSE")) : 300;
   double f0=bpm/60.0, fresp=0.25;
   double cyc = f0*tsec - f0*hrv*cos(2*M_PI*fresp*tsec)/(2*M_PI*fresp);   // latidos transcurridos
   double ph = cyc - floor(cyc);
   // forma de pulso: onda sistólica + onda dícrota (mezcla de gaussianas)
   double g = exp(-pow((ph-0.18)/0.07,2)) + dic*exp(-pow((ph-0.45)/0.08,2)) + exp(-pow((ph-1.18)/0.07,2));
   static double lowFrom = getenv("AMP_LOW_FROM")? atof(getenv("AMP_LOW_FROM")) : -1;
   static double lowTo   = getenv("AMP_LOW_TO")? atof(getenv("AMP_LOW_TO")) : -1;
   static double lowFac  = getenv("AMP_LOW_FACTOR")? atof(getenv("AMP_LOW_FACTOR")) : 1;
   double ampNow = (ms >= lowFrom && ms < lowTo) ? amp*lowFac : amp;
   // R verdadero: (AC_red/DC_red)/(AC_ir/DC_ir) = RTRUE  ->  AC_red = RTRUE * AC_ir * DC_red/DC_ir
   static double rtrue = getenv("RTRUE")? atof(getenv("RTRUE")) : 0.5;
   double redFactor = rtrue * 62000.0/110000.0;
   double pulse = ampNow*(ir?1.0:redFactor)*2.0*(g-0.35);
   double pr = 1.0 + press*(sin(2*M_PI*0.13*ms/1000.0)+0.5*sin(2*M_PI*0.31*ms/1000.0+1));
   double base = (ir?1500:900)*led; double full=(ir?110000:62000)*led*pr;
   double after = (ms >= (uint32_t)g_fingerTo && hover>0) ? hover*(ir?1.0:0.55)*led : base;
   double v = (ms >= (uint32_t)g_fingerTo) ? after + fr*(full-after) - fr*pulse + noise/4
                                            : base + fr*(full-base) - fr*pulse + (fr>0?noise:noise/4);
   if(v<0)v=0; if(v>262143)v=262143; return (uint32_t)v; }
 size_t requestFrom(uint8_t a,size_t n){ por(); if(a!=0x57 || dead()) return 0; rxn=0;rxi=0;
   if(ptr==0x07){ sync(); for(size_t s=0;s<n/6;s++){ uint32_t r=sampleVal(consumed,false), i=sampleVal(consumed,true); consumed++; regs[0x05]=0;
      rx[rxn++]=r>>16; rx[rxn++]=r>>8; rx[rxn++]=r; rx[rxn++]=i>>16; rx[rxn++]=i>>8; rx[rxn++]=i; } return n; }
   if(ptr<=0x06) sync();
   for(size_t i=0;i<n;i++){ uint8_t reg=ptr+i; uint8_t v=regs[reg]; if(reg==0x01 && (regs[0x03]&2)) v=2; if(reg==0x21) v=0; rx[rxn++]=v; }
   return n; }
 int available(){return rxn-rxi;} int read(){return rxi<rxn?rx[rxi++]:-1;}
};
extern TwoWire Wire;
