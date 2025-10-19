#pragma once

#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/adc.h"
#include "status.h"


// ===== Choose one of the following algorithm =====
// #define ALGORITHM_FULL_SUM 1
#define ALGORITHM_AVERAGE 1
// #define ALGORITHM_HIGHEST 1


// ===== ADC / I2S Sampling =====
#define SAMPLE_RATE      16000           // 16 kHz is enough for our application
#define BLOCK_SAMPLES    1024            // ~64 ms block latency
#define I2S_PORT         I2S_NUM_0


// ===== Used Pin for gain =====
#define GAIN_CONTROL_PIN 33


// Analog input (GPIO34 = ADC1_CH6)
#define ADC_UNIT_USED    ADC_UNIT_1
#define ADC_CHANNEL_USED ADC1_CHANNEL_6  // GPIO34
#define ADC_CHANNEL_MC  34               // GPIO34 for Arduino Style


// We want to store a name for the applause result  
#define SOUND_BANDPASS_FRQ  1500.0f


// Gain-Modi Enumeration
typedef enum {
    GAIN_40DB = 0,    // GAIN-Pin = HIGH (VDD)
    GAIN_50DB = 1,    // GAIN-Pin = LOW (GND) 
    GAIN_60DB = 2     // GAIN-Pin = FLOATING (Hi-Z)
} max9814_gain_t;


// ===== Biquad (Bandpass, RBJ constant-skirt gain, Direct Form II Transposed) =====
struct Biquad {
  float a0=1, a1=0, a2=0, b1=0, b2=0;
  float z1=0, z2=0;

  void setBandpass(float fs, float fc, float Q){
    const float w0 = 2.0f * PI * fc / fs;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.0f * Q);

    // RBJ "constant skirt gain" bandpass; peak gain = Q
    const float b0n =   Q * alpha;
    const float b1n =   0.0f;
    const float b2n =  -Q * alpha;
    const float a0n =   1.0f + alpha;
    const float a1n =  -2.0f * cosw0;
    const float a2n =   1.0f - alpha;

    a0 = b0n / a0n;
    a1 = b1n / a0n;
    a2 = b2n / a0n;
    b1 = a1n / a0n;
    b2 = a2n / a0n;
    z1 = z2 = 0.0f;
  }

  inline float process(float x){
    float y = a0 * x + z1;
    z1 = a1 * x - b1 * y + z2;
    z2 = a2 * x - b2 * y;
    return y;
  }
};

struct SoundData {
unsigned long lastTimeMs = 0;
unsigned int dataCount = 0;
float rmsNow = 0;
float rmsMax = 0;
float rmsTotal = 0;
};

struct Applause {
unsigned long id;
SoundData dataBand;
SoundData dataDirect;
unsigned int timebased_measured = 0;
unsigned int timebased_measured_max;
float finalVolume = 0;
float finalPeak = 0;
float finalResult = 0;
char  name[APPLAUSE_NAME_SIZE] = "\0";
};


// ==== Initialize analog part ====
void setup_analog();
void detect_sound(struct SoundData *data, bool count_up);
void reset_sound_data(struct SoundData *data);
void setup_max9814_gain(max9814_gain_t gain);

// ===== I2S (ADC-Mode) Setup =====
void setup_i2s_adc();

// Reads a block of samples (int16_t of ADC/I2S)
size_t read_block(int16_t* dest, size_t maxSamples);

// Calculate blockwise RMS of the bands 
bool block_rms(float& rmsBand, float& rmsDirect);

// Make a copy of the data for other cpu tasks
void MutexCopySoundData(Applause *copyApplause);
void MutexNameEvent(void);
void MutexButtonEvent(bool state);
void MutexUpdateSettings(web_settings_t *settings);

// Application Applause algorithm
void applause_algorithm();

int DebuggerUpdateSettings();
