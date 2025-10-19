#include <ArduinoJson.h>

#include "filesystem.h"
#include "applausometer.h"


Applause stored_counters[DISPLAY_VERY_LAST_COUNTER];

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


int write_file(Applause *pdata, size_t elements) {

  File file = FILESYSTEM.open("/counter.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Error: File could not be opened!");
  }
  else
  {
    for (int i = 0; i < elements; i++)
    {

      char buf[500]; 
      if (strlen(pdata->name) == 0) {
        strcpy(pdata->name, "---");
      }

      snprintf(buf, sizeof(buf), "%lu, %d, %.2f, %.2f, %d, %.2f, %.2f, %s\n", pdata->id, 
                                                                          pdata->dataBand.dataCount, pdata->dataBand.rmsTotal, pdata->dataBand.rmsMax, 
                                                                          pdata->dataDirect.dataCount, pdata->dataBand.rmsTotal, pdata->dataBand.rmsMax,
                                                                          pdata->name);
      file.print(buf);
      pdata++;
    }
    file.close();
    return 0;
  }

  return -1;
}

/*
void read_file(Applause *pdata, size_t elements) {
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

      memmove(pdata+1, pdata, (elements - 1) * sizeof(Applause));      
      // Werte splitten
      int commaIndex = line.indexOf(',');

      pdata->id = line.substring(0, commaIndex).toInt();
      pdata->dataBand.dataCount = line.substring(0, commaIndex).toInt();
      pdata->dataBand.rmsTotal = line.substring(commaIndex + 1).toFloat();
      pdata->dataBand.rmsMax = line.substring(commaIndex + 1).toFloat();

      pdata->dataDirect.dataCount = line.substring(0, commaIndex).toInt();
      pdata->dataBand.rmsTotal = line.substring(commaIndex + 1).toFloat();
      pdata->dataBand.rmsMax = line.substring(commaIndex + 1).toFloat();  

      String label = line.substring(commaIndex + 1);
      //label.trim(); // optional
      label.toCharArray(pdata->name, sizeof(pdata->name));
    }
  }
  file.close();
  Serial.println("=== End of file ===");
}*/

void read_file(Applause *pdata, size_t elements) {
  if (!FILESYSTEM.exists("/counter.csv")) {
    Serial.println("No log file found!");
    return;
  }

  File file = FILESYSTEM.open("/counter.csv", FILE_READ);
  if (!file) {
    Serial.println("Error opening file!");
    return;
  }

  Serial.println("=== Counter-Log ===");
  size_t count = 0;

  char buf[500];  // Buffer of a single line

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // FiFo for older elements
    memmove(pdata + 1, pdata, (elements - 1) * sizeof(Applause));

    // String -> C-String for strtok()
    line.toCharArray(buf, sizeof(buf));

    // Split fields by comma
    char *token = strtok(buf, ",");
    int field = 0;

    Applause temp{};
    
    while (token != NULL) {
      switch (field) {
        case 0:
          temp.id = atoi(token);
          break;
        case 1:
          temp.dataBand.dataCount = atoi(token);
          break;
        case 2:
          temp.dataBand.rmsTotal = atof(token);
          break;
        case 3:
          temp.dataBand.rmsMax = atof(token);
          break;
        case 4:
          temp.dataDirect.dataCount = atoi(token);
          break;
        case 5:
          temp.dataDirect.rmsTotal = atof(token);
          break;
        case 6:
          temp.dataDirect.rmsMax = atof(token);
          break;
        case 7:
          strncpy(temp.name, token, sizeof(temp.name) - 1);
          temp.name[sizeof(temp.name) - 1] = '\0';
          break;
        default:
          break;
      }
      token = strtok(NULL, ",");
      field++;
    }

    // Use new data in into the first array element
    if (field == 8) {
      *pdata = temp;
    }

    count++;
    Serial.printf("Fields: %d, Read #%u: id=%d, count=%d, rmsTotal=%.3f, name=%s\n",
                  field,
                  (unsigned)count,
                  temp.id,
                  temp.dataDirect.dataCount,
                  temp.dataDirect.rmsTotal,
                  temp.name);
  }

  file.close();
  Serial.println("=== End of file ===");
}

void save_record(Applause *data) {
  // Shift all recorded older counters and write the latest
  memmove(stored_counters+1, stored_counters, (DISPLAY_VERY_LAST_COUNTER - 1) * sizeof(Applause));
  memmove(&stored_counters[DISPLAY_COUNTER], data, sizeof(Applause));

  stored_counters[DISPLAY_COUNTER].name[APPLAUSE_NAME_SIZE-1] = 0;

  write_file(data, 1);

  data->id++;
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

/*********************************************************************************************
* @brief  write_debug
*         Write contest data ascii converted as csv file 
* @param  pdata     Pointer to the records
* @param  elements  Number of records
**********************************************************************************************/
int write_debug(Applause *data, int test) {

  File file = FILESYSTEM.open("/debugging.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Error: File could not be opened!");
  }
  else
  {
    char buf[500]; 
    snprintf(buf, sizeof(buf), "%d, %d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f\n", 
              data->dataBand.dataCount, test, 
              data->dataBand.rmsNow, data->dataBand.rmsMax, data->dataBand.rmsTotal,
              data->dataDirect.rmsNow, data->dataDirect.rmsMax, data->dataDirect.rmsTotal,
              data->finalPeak, data->finalResult, data->finalVolume);
    file.print(buf);

    file.close();
    return 0;
  }

  return -1;
}
