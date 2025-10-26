/**
 * @brief Applausometer main program
 * 
 * @author https://oberlab.de 
 * 
 * @version 0.9.0.1
 * 
 * @remarks Used hardware platform: ESP32 DEV KIT C V4
 * 
 */

#include <Arduino.h>
#include "pixel.h"
#include "analog.h"
#include "filesystem.h"
#include "display.h"
#include "button.h"

#include "applausometer.h"



static int display_mode = DISPLAY_LOGO;

extern Biquad soundBandPass;
extern Applause dataApplause;
extern web_events_t system_status;
extern web_settings_t system_settings;

extern Applause stored_counters[DISPLAY_VERY_LAST_COUNTER];


extern Adafruit_NeoPixel strip;


/*********************************************************************************************
* @brief  setup
*         Initialize all hardware and software parts
*         - This application is using an analog input for measuring the volume
*         - A button and a small display are used to control the application
*         - Large LED strips are used to display the status 
*         - A file system is available to store the last logged counts
*         - A web page can be reached by the small wifi access point to preset some details
**********************************************************************************************/
void setup_applausometer() {

  pinMode(LED_TEST,OUTPUT);
  digitalWrite(LED_TEST, true);

  Serial.begin(115200);
  delay(200);
  
  Serial.println("\nApplausometer started");


  // =================================
  // Initialize analog part (microphone)
  // =================================
  setup_i2s_adc();
  setup_analog();
  setup_button();


  // =================================
  // Initialize small display
  // =================================
  setup_display();
  display_print_image(false);


  // =================================
  // Initialize LED strip
  // =================================
  strip.begin();
  strip.show();                     // clear all LED
  strip.setBrightness(128);         // 0..255

  
  // =================================
  // Check existing logged counts
  // =================================
  read_file(stored_counters, DISPLAY_VERY_LAST_COUNTER);
  

  digitalWrite(LED_TEST, false);
}

/*********************************************************************************************
 * @brief loop_applausometer
 *        main loop of the application
 * 
 *********************************************************************************************/

void loop_applausometer() {
  
  static float progress_hori = 0;
  static float progress_vert = 0;

  static unsigned int buttonmode = 0;
  static unsigned int page = 0;
  static bool measurement_active = true;
  static bool resetStripLED = false;

  static unsigned long tMonitorLED = 0;
  static unsigned long tMeasurement = 0;
  static unsigned long tMonitorDisplay = 0;

  static ButtonEvent btn_event = BUTTON_NONE;
  static ButtonEvent btn_state = BUTTON_NONE;

  const unsigned long now = millis();

  dataApplause.dataDirect.rmsNow = 0.0f, dataApplause.dataBand.rmsNow = 0.0f;


  
  // =================================
  // Sound detection
  // =================================
  if (!block_rms(dataApplause.dataBand.rmsNow, dataApplause.dataDirect.rmsNow)) {
    return;
  }
  
  detect_sound(&dataApplause.dataDirect, measurement_active);
  detect_sound(&dataApplause.dataBand, measurement_active);
  vTaskDelay(1);


  // =================================
  // Here is the main algorithm of the measurement
  // =================================
  applause_algorithm();


  // =================================
  // Time based measurement 
  // ================================= 
  if (now - tMeasurement > 1000) {
    dataApplause.timebased_measured++;

    tMeasurement = now;

    // Print debug information
    Serial.printf("RMS  Direct=%.4f  Band=%.4f  Total: %.1f| RMS max Direct=%.4f  Band=%.4f | Cnt: Direct=%.1f, Band=%.1f\n", 
          dataApplause.dataDirect.rmsNow, dataApplause.dataBand.rmsNow, 
          dataApplause.finalVolume, 
          dataApplause.dataDirect.rmsMax, dataApplause.dataBand.rmsMax, 
          dataApplause.dataDirect.rmsTotal, dataApplause.dataBand.rmsTotal);

    //Max Uthoff Test!!! Fliegt dann wieder raus!!! 
    //int test = DebuggerUpdateSettings();
    //write_debug(&dataApplause, test);
    //Max Uthoff Test!!! Fliegt dann wieder raus!!!
  }

  if (dataApplause.timebased_measured > dataApplause.timebased_measured_max) {
    if (measurement_active) {
      updateList(&dataApplause, false, true);
      Serial.printf("Measurement completed!\n");
    }
    measurement_active = false;
    dataApplause.timebased_measured = dataApplause.timebased_measured_max;
  }  


  // =================================
  // LED strip monitoring
  // =================================
  if (now - tMonitorLED > 50) {
    tMonitorLED = now;
    showStripLED((dataApplause.finalVolume), resetStripLED);
    resetStripLED = false;
    vTaskDelay(1);
  }


  // =================================
  // Button control
  // =================================
  btn_event = checkButton(&btn_state, true);
  
  switch (btn_event) {
    case BUTTON_SHORT:
      Serial.printf("Short pressed \n");
      if (buttonmode == DISPLAY_EXTENDED_INFO)      // Extended diagnosis pages
      {
        page++;
        Serial.printf("Page up: %d\n", page);
        if (page > DISPLAY_SYSTEM)
          page = DISPLAY_COUNTER;
      }
      break;

    case BUTTON_LONG2S:
      Serial.printf("Medium pressed (2s)\n");
      if (buttonmode == DISPLAY_NORMAL)             // Reset counters
      {
        Serial.printf("Reset by button!\n");

        updateList(&dataApplause, true, false);

        dataApplause.timebased_measured = 0;
        measurement_active = true;
        resetStripLED = true;
      }      
      break;

    case BUTTON_LONG5S:
      Serial.printf("Long pressed (5s)\n");

      page = DISPLAY_COUNTER;
      buttonmode++;
      Serial.printf("Mode up: %d\n", buttonmode);
      if (buttonmode > DISPLAY_EXTENDED_INFO)
      { 
        buttonmode = DISPLAY_NORMAL;
      }
      break;

    default:
      updateList(&dataApplause, false, false);
      break;
  }
  vTaskDelay(1);


  // =================================
  // Small display control
  // =================================  
  // Show a small progress bar on the right side to visualize the  putton press time 
  progress_vert = (float)(btn_state) / BUTTON_LONG5S; // 0, 0.33, 0.66, 1.0 
  
  // Show a small progress bar on the bottom line to visualize the elapsed time (%) of the measurement 
  progress_hori = (float)(dataApplause.timebased_measured) / dataApplause.timebased_measured_max; // 0...1 

  // Show the correct display of the current mode
  if (now - tMonitorDisplay > 101) {
    tMonitorDisplay = now;
    if (buttonmode == DISPLAY_LOGO && now > 3000)
      buttonmode = DISPLAY_NORMAL;

    switch (buttonmode) {
      case DISPLAY_LOGO:                // Start splash
        display_print_image(false); 
      break;

      case DISPLAY_EXTENDED_INFO:       // Show history and system info
        if (page < DISPLAY_SYSTEM)
          display_print_counter(stored_counters, page, DISPLAY_VERY_LAST_COUNTER, progress_vert);
        else
          display_print_image(true);
          //display_print_system();
      break;

      case DISPLAY_NORMAL:              // Show current measurement
      default:                  
        // Example show time based integrated sum
        display_print_value(dataApplause.finalResult, 
                            dataApplause.finalVolume,
                            dataApplause.finalPeak, 
                            progress_hori, progress_vert);
        break;
    }
  }


  // =================================
  // Web control
  // =================================  
  // Check if new participant name arrived from web
  if (system_status.name_updated) {
    mutexNameEvent();  
    Serial.printf("Name set by web: %s\n", dataApplause.name);
  }

  if (system_status.button_reset) {           // Reading of the value without semaphore
          Serial.printf("Reset by web!\n");

          //updateList(&dataApplause, true, false);    // See display button management, which controls updateList(..., false) also for normal updates 

          mutexButtonEvent(false);            // To write the value we will use a semaphore

          dataApplause.timebased_measured = 0;
          measurement_active = true;
          resetStripLED = true;

          strcpy(dataApplause.name, "---");
  }
}
