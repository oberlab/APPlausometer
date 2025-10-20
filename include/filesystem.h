#pragma once

#include <ArduinoJson.h>
#include "fs_compat.h"
#include "analog.h"


int  write_file(Applause *pdata, size_t elements);
void read_file(Applause *pdata, size_t elements);
bool fs_mount(bool ShowInfo);

void updateList(Applause *data, bool shift);
bool loadSettings(web_settings_t *settings);

int write_debug(Applause *data, int test);