#pragma once

#include <FS.h>

#ifdef USE_FFAT
#include <FFat.h>
#define FILESYSTEM FFat
#else
#include <SPIFFS.h>
#define FILESYSTEM SPIFFS
#endif

