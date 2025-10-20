#include <ArduinoJson.h>

#include "filesystem.h"
#include "applausometer.h"


Applause stored_counters[DISPLAY_VERY_LAST_COUNTER];

/*********************************************************************************************
* @brief  SPIFFS file system management
* 
* @param  ShowInfo  Show debugging information
*
* @result true, if the file system could be mounted successfully
*          
**********************************************************************************************/
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


/*********************************************************************************************
* @brief  write_file writes the latest log data to the SPIFFS
* 
* @param  pdata     Target structure 
* @param  elements  Number of elements to write
*
* @result 0 if ok, -1 in case of an error
*
* @remarks Typically, a single data record will be added to an existing file or a new file
*          will be created.
**********************************************************************************************/
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

      snprintf(buf, sizeof(buf), "%lu, %s, %.2f, %.2f, %d, %.2f, %.2f, %d, %.2f, %.2f\n", pdata->id, pdata->name, pdata->finalPeak, pdata->finalResult,
                                                                          pdata->dataBand.dataCount, pdata->dataBand.rmsTotal, pdata->dataBand.rmsMax, 
                                                                          pdata->dataDirect.dataCount, pdata->dataBand.rmsTotal, pdata->dataBand.rmsMax);
      file.print(buf);
      pdata++;
    }
    file.close();
    return 0;
  }

  return -1;
}


/*********************************************************************************************
* @brief  read_file reads the logged data from the SPIFFS, if available and in a valid 
*         format.
* 
* @param  pdata     Target array (stored_counters) 
* @param  elements  Target array elements
*
* @result void
*
* @remarks Only the latest stored elements will be buffered in the stored_counters FIFO buffer 
*          
**********************************************************************************************/
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
          strncpy(temp.name, token, sizeof(temp.name) - 1);
          temp.name[sizeof(temp.name) - 1] = '\0';
          break;        
        case 2:
          temp.finalPeak = atoi(token);
          break;
        case 3:
          temp.finalResult = atof(token);
          break;        
        case 4:
          temp.dataBand.dataCount = atoi(token);
          break;
        case 5:
          temp.dataBand.rmsTotal = atof(token);
          break;
        case 6:
          temp.dataBand.rmsMax = atof(token);
          break;
        case 7:
          temp.dataDirect.dataCount = atoi(token);
          break;
        case 8:
          temp.dataDirect.rmsTotal = atof(token);
          break;
        case 9:
          temp.dataDirect.rmsMax = atof(token);
          break;
        default:
          break;
      }
      token = strtok(NULL, ",");
      field++;
    }

    // Use new data in into the first array element
    if (field == 10) {
      *pdata = temp;
    }

    count++;
    Serial.printf("id=%d, name=%s, peak=%.2f, result=%.2f\n",
                  temp.id,
                  temp.name,
                  temp.finalPeak,
                  temp.finalResult);
  }

  file.close();
  Serial.println("=== End of file ===");
}


/*********************************************************************************************
* @brief  updateList managed the measured data in a short list and writes new records to the 
*         log file.
* 
* @param  data  Latest applause measurement data
* @param  shift If true, a record will be shifted in the list and stored 
*
* @result void
*
* @remarks The local stored FIFO buffer will be updated, too. This data are used for display
*          and web page.
**********************************************************************************************/
void updateList(Applause *data, bool shift) {
  
  // Shift all recorded older counters and write the latest
  if (shift) {
    memmove(stored_counters+1, stored_counters, (DISPLAY_VERY_LAST_COUNTER - 1) * sizeof(Applause));
  }
  //Copy the current information to the first record in any case
  memmove(&stored_counters[DISPLAY_COUNTER], data, sizeof(Applause));

  //Only store if shifted a new record
  if (shift) {
    write_file(data, 1);

    //Test print
    Applause *pStored = stored_counters; 
    for (int i = 0; i < DISPLAY_VERY_LAST_COUNTER; i++) {
      Serial.printf("id=%d, name=%s, peak=%.2f, result=%.2f\n",
                    pStored->id,
                    pStored->name,
                    pStored->finalPeak,
                    pStored->finalResult);
      pStored++;
    }
    data->id++;
    reset_sound_data(&data->dataDirect);
    reset_sound_data(&data->dataBand);
  }
}


/*********************************************************************************************
* @brief  saveSettings writes the configuration to the SPIFFS 
  
* @param  cfg  Json configured settings
*
* @result void
*
**********************************************************************************************/
void saveSettings(JsonVariant cfg) {
  File file = FILESYSTEM.open("/config.json", FILE_WRITE);
  if (!file) {
    Serial.println("Error opening settings!");
    return;
  }

  serializeJson(cfg, file);
  file.close();
}


/*********************************************************************************************
* @brief  loadSettings reads the configuration from the SPIFFS if possible
*         If no valid file could be found, the settings are unchanged  
* @param  settings  Settings to update
*
* @result true, if settings are updated, false in case of error or not found
*
**********************************************************************************************/
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
