#include <ArduinoJson.h>

#include "filesystem.h"
#include "applausometer.h"


SoundStatistic stored_counters[DISPLAY_VERY_LAST_COUNTER];

bool fs_mount(bool ShowInfo) {
    bool fs_ok = false;

    if ((fs_ok = FILESYSTEM.begin(true)) == false) {
      Serial.println("An Error has occurred while mounting filesystem");
      // create filesystem
      if (!FILESYSTEM.format()) {
        Serial.println("Failed to format filesystem");
      }
      else
      {
        Serial.println("Filesystem formatted successfully");
        fs_ok = true;
      }
      if (!FILESYSTEM.begin(true)) {
        Serial.println("Failed to mount filesystem after formatting");
      }
    }

    if (fs_ok && ShowInfo) {
      size_t total = FILESYSTEM.totalBytes();
      size_t used  = FILESYSTEM.usedBytes();

      Serial.println("=== FS Info ===");
      Serial.print("Total size: ");
      Serial.print(total);
      Serial.println(" Bytes");

      Serial.print("Used: ");
      Serial.print(used);
      Serial.println(" Bytes");

      Serial.print("Available: ");
      Serial.print(total - used);
      Serial.println(" Bytes");
      Serial.println("====================");
    }
    return fs_ok;
}


int write_file(SoundStatistic *pdata, size_t elements) {

  File file = FILESYSTEM.open("/counter.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Error: File could not be opened!");
  }
  else
  {
    for (int i = 0; i < elements; i++)
    {

      char buf[500]; 
      snprintf(buf, sizeof(buf), "%lu, %d, %.2f, %.2f, %d, %.2f, %.2f\n", pdata->id, pdata->ClapCount, pdata->ClapTotalClap, pdata->ClapMaxRms, pdata->StompCount, pdata->StompTotalClap, pdata->StompMaxRms);
      file.print(buf);
      pdata++;
    }
    file.close();
    return 0;
  }

  return -1;
}


void read_file(SoundStatistic *pdata, size_t elements) {
  if (!FILESYSTEM.exists("/counter.csv")) {
    Serial.println("No log file found!");
    return;
  }

  File file = FILESYSTEM.open("/counter.csv", FILE_READ);
  if (!file) {
    Serial.println("Error opening a file!");
    return;
  }

  Serial.println("=== Counter-Log ===");
  size_t count = 0;

  // Read all lines and remember the last line
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      Serial.println(line);

      memmove(pdata+1, pdata, (elements - 1) * sizeof(SoundStatistic));      
      // Werte splitten
      int commaIndex = line.indexOf(',');
      pdata->id = line.substring(0, commaIndex).toInt();
      pdata->ClapCount = line.substring(0, commaIndex).toInt();
      pdata->ClapMaxRms = line.substring(commaIndex + 1).toFloat();
      pdata->ClapTotalClap = line.substring(commaIndex + 1).toFloat();

      pdata->StompCount = line.substring(0, commaIndex).toInt();
      pdata->StompMaxRms = line.substring(commaIndex + 1).toFloat();
      pdata->StompTotalClap = line.substring(commaIndex + 1).toFloat();    

    }
  }
  file.close();
  Serial.println("=== End of file ===");
}

void save_record(Applause *data) {
  // Shift all recorded older counters and write the latest
  memmove(stored_counters+1, stored_counters, (DISPLAY_VERY_LAST_COUNTER - 1) * sizeof(SoundStatistic));

  stored_counters[DISPLAY_COUNTER].ClapCount = data->dataBand.dataCount; 
  stored_counters[DISPLAY_COUNTER].ClapMaxRms = data->dataBand.rmsMax; 
  stored_counters[DISPLAY_COUNTER].ClapTotalClap = data->dataBand.rmsTotal; 

  stored_counters[DISPLAY_COUNTER].StompCount = data->dataDirect.dataCount; 
  stored_counters[DISPLAY_COUNTER].StompMaxRms = data->dataDirect.rmsMax; 
  stored_counters[DISPLAY_COUNTER].StompTotalClap = data->dataDirect.rmsTotal;

  memcpy(stored_counters[DISPLAY_COUNTER].name, data->name, APPLAUSE_NAME_SIZE);
  stored_counters[DISPLAY_COUNTER].name[APPLAUSE_NAME_SIZE-1] = 0;

  stored_counters[DISPLAY_COUNTER].id++;      // Old element contains still the last id 
  write_file(stored_counters, DISPLAY_VERY_LAST_COUNTER);

  reset_sound_data(&data->dataDirect);
  reset_sound_data(&data->dataBand);
}


void saveSettings(JsonVariant cfg) {
  File file = FILESYSTEM.open("/config.json", FILE_WRITE);
  if (!file) {
    Serial.println("Error opening settings!");
    return;
  }

  serializeJson(cfg, file);
  file.close();
}


bool loadSettings(web_settings_t *settings) {
  int config_missing = 0;

  File file = FILESYSTEM.open("/config.json", FILE_READ);
  if (!file) {
    Serial.println("Error opening settings!");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("Error parsing settings: ");
    Serial.println(error.c_str());
    return false;
  }

  if (doc.containsKey("frq")) {
    settings->frq = doc["frq"].as<int>();
  }
  else {config_missing++;}

  if (doc.containsKey("gain")) {
    settings->gain_db = doc["gain"].as<int>();
  }
  else {config_missing++;}
  
  if (doc.containsKey("duration")) {
    settings->duration = doc["duration"].as<unsigned long>();
  }
  else {config_missing++;}

  return (config_missing == 0);

}

