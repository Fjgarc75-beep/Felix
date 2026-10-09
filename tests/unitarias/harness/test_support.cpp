// =====================================================================
//  test_support.cpp - Símbolos globales del núcleo simulado para pruebas
//  Sustituye a cores/fake/main.cpp: define lo mismo PERO sin main(), para
//  que cada ejecutable de pruebas tenga su propio main().
// =====================================================================
#include "Arduino.h"
#include <cstdlib>

uint32_t g_us = 0;
S Serial;
EspS ESP;
void (*g_dumpHook)(uint32_t) = 0;
const char *g_script = "";
uint32_t g_scriptAt[16];
int g_scriptPos = 0;
int g_fingerFrom = 12000, g_fingerTo = 21000;
int g_ledWritesBeforeSetup = 0;
bool fingerOn(uint32_t ms) { return ms >= (uint32_t)g_fingerFrom && ms < (uint32_t)g_fingerTo; }
