#include "analog.h"


extern SemaphoreHandle_t applause_mutex;
extern web_events_t system_status;
extern web_settings_t system_settings;

// =================================
// =     Define band pass
// =================================
const float         SOUND_BANDPASS_QUALITY  = 1.0f;
const float         THRESHOLD_SOUND = 0.015f;
const unsigned long DEBOUNCE_SOUND_MS = 180;


Applause dataApplause;
Biquad soundBandPass;


/*********************************************************************************************
* @brief  setup_analog
*         Initialize analog signal related data
*         Used for the microphone
**********************************************************************************************/
void setup_analog(){
  // Config bands
  soundBandPass.setBandpass((float)SAMPLE_RATE, SOUND_BANDPASS_FRQ , SOUND_BANDPASS_QUALITY );
  
  dataApplause.id = 0;
  reset_sound_data(&dataApplause.dataDirect);
  reset_sound_data(&dataApplause.dataBand);
}

/*********************************************************************************************
* @brief  Set a gain mode of the MAX9814
*         Used for multithreading operations to the same data  
*
* @param  gain Use the pre-defined constants e.g. GAIN_40DB
*
**********************************************************************************************/
void setup_max9814_gain(max9814_gain_t gain) {
    switch(gain) {
        case GAIN_40DB:
            // 40dB: GAIN-Pin auf HIGH (VDD)
            pinMode(GAIN_CONTROL_PIN, OUTPUT);
            digitalWrite(GAIN_CONTROL_PIN, HIGH);
            Serial.println("MAX9814: Set to 40dB gain (Speech/Near field)");
            break;
            
        case GAIN_50DB:
            // 50dB: GAIN-Pin auf LOW (GND)
            pinMode(GAIN_CONTROL_PIN, OUTPUT);
            digitalWrite(GAIN_CONTROL_PIN, LOW);
            Serial.println("MAX9814: Set to 50dB gain (Applause/Medium field)");
            break;
            
        case GAIN_60DB:
            // 60dB: GAIN-Pin floating (High-Z)
            pinMode(GAIN_CONTROL_PIN, OUTPUT_OPEN_DRAIN);
            digitalWrite(GAIN_CONTROL_PIN, HIGH);
            Serial.println("MAX9814: Set to 60dB gain (Quiet sounds/Far field)");
            break;
    }
    
    // Short delay to stabilize the microphone 
    delay(5);
}

/*********************************************************************************************
* @brief  mutexCopySoundData
*         Used for multithreading operations to the same data  
*
* @param  copyApplause We will copy the original to this structure
*
**********************************************************************************************/
void mutexCopySoundData(Applause *copyApplause) {
  if (xSemaphoreTake(applause_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Critical data exchange  
      memcpy(copyApplause, &dataApplause, sizeof(Applause));
      xSemaphoreGive(applause_mutex); 
  }
}

/*********************************************************************************************
* @brief  mutexCopySettings
*         Used for multithreading operations to the same data  
*
* @param  settings We will directly modify the values
*
**********************************************************************************************/
void mutexUpdateSettings(web_settings_t *settings) {
  if (xSemaphoreTake(applause_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Critical data exchange  
    soundBandPass.setBandpass((float)SAMPLE_RATE, settings->frq, SOUND_BANDPASS_QUALITY );
    dataApplause.timebased_measured_max = settings->duration;
    switch (settings->gain_db) {
      case 40:
        setup_max9814_gain(GAIN_40DB);
        break;
      case 50:
        setup_max9814_gain(GAIN_50DB);
        break;      
      case 60:
      default:
        setup_max9814_gain(GAIN_60DB);
        break;
    }
    xSemaphoreGive(applause_mutex); 
  }
}

/*********************************************************************************************
* @brief  mutexButtonEvent
*         Used for multithreading operations to the same data  
*
* @param  state We will directly modify the state
*
**********************************************************************************************/
void mutexButtonEvent(bool state) {
  if (xSemaphoreTake(applause_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Critical data exchange
        system_status.button_reset = state;
      xSemaphoreGive(applause_mutex);
  } 
}

/*********************************************************************************************
* @brief  MutexNameEvent
*         Used for multithreading operations to the same data  
*
**********************************************************************************************/
void mutexNameEvent(void) {
  if (xSemaphoreTake(applause_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Copy name to current applause data
    strncpy(dataApplause.name, system_status.participant_name, APPLAUSE_NAME_SIZE - 1);
    dataApplause.name[APPLAUSE_NAME_SIZE - 1] = '\0';
    system_status.name_updated = false;
    xSemaphoreGive(applause_mutex);
  }
}

/*********************************************************************************************
* @brief  detect_sound
*         Quiet noise will not be counted 
*
* @param  state We will directly modify the state
*
**********************************************************************************************/
void detect_sound(struct SoundData *data, bool count_up) {    
  
  const unsigned long now = millis();

  // Detect noise to count
  if (data->rmsNow > THRESHOLD_SOUND && (now - data->lastTimeMs) > DEBOUNCE_SOUND_MS) {
    data->lastTimeMs = now;
    if(count_up) {
      data->dataCount++;
      data->rmsTotal+=data->rmsNow;
      if (data->rmsNow > data->rmsMax) {
        data->rmsMax = data->rmsNow;
    } 
    }
  }
};

/*********************************************************************************************
* @brief  reset_sound_data
*         Reset all SoundData structure values
*
* @param  data Measured values of a single band pass
*
**********************************************************************************************/
void reset_sound_data(struct SoundData *data) {   
  data->dataCount = 0;
  data->rmsTotal = 0;
  data->rmsMax = 0; 
};

/*********************************************************************************************
* @brief  I2S (ADC-Mode) Setup
*         The analog value can be read in a ESP32 via i2s process
*         Used as automatic process but this is not a real i2c communication
*
**********************************************************************************************/
void setup_i2s_adc(){
  // Configure ADC (Width & damping -> 0..3.3V )
  adc1_config_width(ADC_WIDTH_BIT_12);
  analogSetPinAttenuation(ADC_CHANNEL_MC, ADC_11db);

  // =================================
  // I2S in ADC mode
  // =================================
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,      // ADC writes 12 Bit into 16 Bit registers
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,      // Dummy: ADC is using mono
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BLOCK_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL));
  ESP_ERROR_CHECK(i2s_set_adc_mode(ADC_UNIT_USED, ADC_CHANNEL_USED));
  // Start ADC via I2S
  ESP_ERROR_CHECK(i2s_adc_enable(I2S_PORT));

  // Optimize frequency (workaround for I2S/ADC drift)
  ESP_ERROR_CHECK(i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO));
}

/*********************************************************************************************
* @brief  read_block
*         Reads a block of samples (int16_t of ADC/I2S)
* @param  dest        array for data
* @param  maxSamples  number of elements of dest
*
**********************************************************************************************/
size_t read_block(int16_t* dest, size_t maxSamples) {
  size_t bytesRead = 0;
  // Note: i2s_read sends number of bytes
  esp_err_t ok = i2s_read(I2S_PORT, (void*)dest, maxSamples * sizeof(int16_t), &bytesRead, portMAX_DELAY);
  if (ok != ESP_OK) return 0;
  return bytesRead / sizeof(int16_t);
}

bool block_rms(float& rmsBand, float& rmsDirect) {
    static int16_t raw[BLOCK_SAMPLES];
    size_t n = read_block(raw, BLOCK_SAMPLES);
    if (n == 0) return false;

    double sumSqBand = 0.0;
    double sumSqTotal = 0.0;        // statt sumSqClap

    for (size_t i = 0; i < n; ++i) {
        uint16_t s12 = (uint16_t)raw[i] & 0x0FFF;          // 12-Bit
        float x = ((int)s12 - 2048) / 2048.0f;             // sub DC offset

        float band = soundBandPass.process(x);             // Bandpass for optional sound range
        sumSqBand  += (double)band * band;

        sumSqTotal  += (double)x * x;                      // Sum of MS
    }

    rmsBand = sqrtf((float)(sumSqBand / (double)n));
    rmsDirect  = sqrtf((float)(sumSqTotal / (double)n));   // Direct volume
    return true;
}

/*********************************************************************************************
* @brief  applause_algorithm
*         Calculates our applausometer results which should be used for the contest
*         Because it is not really clear which way is the best, we support different calcs
*
**********************************************************************************************/
void applause_algorithm() {
  dataApplause.finalResult = dataApplause.dataDirect.rmsTotal;

  // Different ideas for the calculation:
#ifdef ALGORITHM_FULL_SUM 
  dataApplause.finalVolume = dataApplause.dataBand.rmsNow + dataApplause.dataDirect.rmsNow;
#endif

#ifdef ALGORITHM_DIRECT_ONLY 
  dataApplause.finalVolume = dataApplause.dataDirect.rmsNow;
#endif

#ifdef ALGORITHM_HIGHEST 
  if (dataApplause.dataBand.rmsNow > dataApplause.dataDirect.rmsNow) {
    dataApplause.finalVolume = dataApplause.dataBand.rmsNow;
  } else {
    dataApplause.finalVolume = dataApplause.dataDirect.rmsNow;
  }
#endif

  dataApplause.finalPeak   = dataApplause.dataDirect.rmsMax;
}


/*********************************************************************************************
* @brief  Debugging life test
*
**********************************************************************************************/
int debuggerUpdateSettings() {
  static int test = 0;

  test++;
  if (test == 2) {
    dataApplause.timebased_measured_max = 9999;
    setup_max9814_gain(GAIN_60DB);
  }  
  if (test == 42) {
    dataApplause.timebased_measured_max = 9999;
    setup_max9814_gain(GAIN_50DB);
  }
  if (test == 82) {
    dataApplause.timebased_measured_max = 9999;
    setup_max9814_gain(GAIN_40DB);
  }
  if (test == 122) {
    dataApplause.timebased_measured_max = 9999;
    setup_max9814_gain(GAIN_60DB);
  }  
  if (test == 162) {
    dataApplause.timebased_measured_max = 9999;
    setup_max9814_gain(GAIN_50DB);
  }
  if (test == 202) {
    dataApplause.timebased_measured_max = 9999;
    setup_max9814_gain(GAIN_40DB);
  }
  if (test == 242) {
    test = 0;
  }
  return test;
}