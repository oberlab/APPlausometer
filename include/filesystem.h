#pragma once

#include <ArduinoJson.h>
#include "fs_compat.h"
#include "analog.h"


struct SoundStatistic {
unsigned long id= 0;
unsigned int StompCount = 0;
unsigned int ClapCount = 0;
float StompTotalClap = 0;
float ClapTotalClap = 0;
float StompMaxRms = 0;
float ClapMaxRms = 0;
char  name[APPLAUSE_NAME_SIZE] = "\0";
};


int  write_file(SoundStatistic *pdata, size_t elements);
void read_file(SoundStatistic *pdata, size_t elements);
bool fs_mount(bool ShowInfo);

void save_record(Applause *data);

void saveSettings(JsonVariant cfg);
bool loadSettings(web_settings_t *settings);

