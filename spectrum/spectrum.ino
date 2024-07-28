// 32 band audio spectrum visualizer for RP2040
//
// Copyright (C) 2024 Ding Zhaojie <zhaojie_ding@msn.com>
//
// This work is licensed under the terms of the GNU GPL, version 2 or later.
// See the COPYING file in the top-level directory.

#include <arduinoFFT.h>
#include <MD_MAX72xx.h>

// set true to calibrate the EQ array values
constexpr bool CALIBRATION = false;

// board configuration
constexpr int PIN_CS = 5,
              PIN_CLK = 2,
              PIN_DIN = 3,
              PIN_MIC = 28;

// MAX7219 configuration
constexpr int NUM_MATRIX = 4, NUM_DOT = 8;
static MD_MAX72XX disp = MD_MAX72XX(MD_MAX72XX::FC16_HW, PIN_DIN, PIN_CLK, PIN_CS, NUM_MATRIX);
constexpr int bar[]{
  0b00000000,
  0b10000000,
  0b11000000,
  0b11100000,
  0b11110000,
  0b11111000,
  0b11111100,
  0b11111110,
  0b11111111,
};

// fft configuration, about 71 fps @ 32kHz, 128 sampling
constexpr int SAMPLES = 128,
              SAMPLING_FREQ = 32000;

// display configuration
constexpr int NUM_BANDS = NUM_DOT * NUM_MATRIX,
              MAX_VALUE = 80;

// amplifier and EQ
constexpr double AMPLITUDE = 4;
static double EQ[NUM_BANDS]{
  0.408414,
  0.289516,
  0.187662,
  0.123428,
  0.118204,
  0.136386,
  0.132653,
  0.196402,
  0.109531,
  0.128709,
  0.174637,
  0.227107,
  0.149161,
  0.180958,
  0.232711,
  0.190991,
  0.243680,
  0.267034,
  0.512626,
  0.522463,
  0.613265,
  0.810226,
  0.934661,
  0.819630,
  0.902442,
  0.777110,
  0.838900,
  0.555142,
  0.556224,
  0.625557,
  1.067809,
  1.691705,
};

static unsigned int sampling_period_us;
static double vReal[SAMPLES], vImag[SAMPLES], bands[NUM_BANDS];
static arduinoFFT fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

static void disp_init() {
  disp.begin();
  disp.control(MD_MAX72XX::UPDATE, 0);
  disp.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY / 8);
}

static void eq_init() {
  for (int i = 0; i < NUM_BANDS; i++) {
    EQ[i] /= AMPLITUDE;
  }
}

void setup() {
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
  eq_init();
  Serial.begin(921600);
  disp_init();
}

static void do_sampling() {
  memset(vImag, 0, sizeof(vImag));
  for (int i = 0; i < SAMPLES; i++) {
    auto newTime = micros();
    vReal[i] = analogRead(PIN_MIC);
    while ((micros() - newTime) < sampling_period_us) {
      /* chill */
    }
  }
}

static void calc_fft() {
  fft.DCRemoval();
  fft.Windowing(FFTWindow::Hamming, FFTDirection::Forward);
  fft.Compute(FFTDirection::Forward);
  fft.ComplexToMagnitude();
}

// 48 FFT bin to 32 spectrum band
static int bin_to_band(int bin) {
  switch (bin) {
    case 0 ... 25:
      return bin;
    case 26 ... 27:  // 2
      return 26;
    case 28 ... 29:  // 2
      return 27;
    case 30 ... 33:  // 4
      return 28;
    case 34 ... 37:  // 4
      return 29;
    case 38 ... 42:  // 5
      return 30;
    case 43 ... 47:  // 5
      return 31;
    default:
      return 31;
  }
}

static void collect_bands() {
  memset(bands, 0, sizeof(bands));

  // only need the <= 24kHz part
  for (int i = 0; i < 48; i++) {
    bands[bin_to_band(i)] += vReal[i];
  }
}

static void calibration() {
  static int cnt;
  static double band_max[NUM_BANDS];

  if (cnt++ < 1600) {
    for (int i = 0; i < NUM_BANDS; i++) {
      band_max[i] = max(band_max[i], bands[i]);
    }
  } else {
    Serial.println("-------------------------------");
    double base = MAX_VALUE * AMPLITUDE * 1.2;
    for (int i = 0; i < NUM_BANDS; i++) {
      Serial.printf(" %f,", base / band_max[i]);
    }
    Serial.println("\n-------------------------------");
    cnt = 0;
    memset(band_max, 0, sizeof(band_max));
  }
}

static void post_processing() {
  if (CALIBRATION) {
    calibration();
  }
  for (int i = 0; i < NUM_BANDS; i++) {
    bands[i] *= EQ[i];
  }
}

static void display() {
  double max_v = 0;
  for (int i = 0; i < NUM_BANDS; i++) {
    max_v = max(max_v, bands[i]);
  }

  // filter noise
  if (max_v < MAX_VALUE / NUM_DOT * 2) {
    memset(bands, 0, sizeof(bands));
  }
  max_v = max(max_v, MAX_VALUE);

  for (int i = 0; i < NUM_BANDS; i++) {
    disp.setColumn(NUM_BANDS - i - 1, bar[map(bands[i], 0, max_v, 0, 8)]);
  }
  disp.update();
}

void loop() {
  // auto t = micros();
  do_sampling();
  calc_fft();
  collect_bands();
  post_processing();
  display();
  // Serial.printf("%lu fps\n", 1000000 / (micros() - t));
}
