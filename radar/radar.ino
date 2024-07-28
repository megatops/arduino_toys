// Ultrasonic Fire Control Radar for RP2040
//
// Copyright (C) 2023 Ding Zhaojie <zhaojie_ding@msn.com>
//
// This work is licensed under the terms of the GNU GPL, version 2 or later.
// See the COPYING file in the top-level directory.

#include <DistanceSensor.h>
#include <Servo.h>

// board configurations
const int SONAR_L_TRIG = 10;
const int SONAR_L_ECHO = 11;
const int SONAR_R_TRIG = 2;
const int SONAR_R_ECHO = 3;
const int SERVO = 6;

// distance configurations (in cm)
//
//          T (target)
//
//
//    L            R            S
//  sonar        sonar        servo
//
const double D_LR = 8.9;   // distance between 2 sonars (Rx)
const double D_RS = 10.6;  // distance between right sonar (Rx) and servo
const double D_LS = D_LR + D_RS;

const uint32_t SONAR_DELAY = 50;  // delay 50ms between detections
static DistanceSensor sonarL(SONAR_L_TRIG, SONAR_L_ECHO);
static DistanceSensor sonarR(SONAR_R_TRIG, SONAR_R_ECHO);
static Servo servo;

void setup() {
  pinMode(SONAR_L_TRIG, OUTPUT);
  pinMode(SONAR_L_ECHO, INPUT);
  pinMode(SONAR_R_TRIG, OUTPUT);
  pinMode(SONAR_R_ECHO, INPUT);
  pinMode(SERVO, OUTPUT);

  Serial.begin(921600);
  servo.attach(SERVO, 500, 2500);  // for SG90 servo
}

static double pow2(double x) {
  return x * x;
}

static double degree(double rad) {
  return rad * 180 / M_PI;
}

static double detect_single(double dL, double dR) {
  // the target is out of the range of one sonar
  return degree(dL < dR ? atan(dL / D_LS) : atan(dR / D_RS));
}

static double detect() {
  // use triangulation to calculate the target position. if the result
  // is impossible, fall back to calculate with the nearest sonar.
  double dL = sonarL.getCM();
  delay(SONAR_DELAY);
  double dR = sonarR.getCM();
  delay(SONAR_DELAY);
  Serial.printf("dL: %f, dR: %f\n", dL, dR);

  if (dL + dR <= D_LR || dL + D_LR <= dR || dR + D_LR <= dL) {
    // cannot form a triangle
    return detect_single(dL, dR);
  }

  // calculate angle TLR with cosine law
  double cosL = (pow2(dL) + pow2(D_LR) - pow2(dR)) / (2 * dL * D_LR);

  // calculate the distance between target and servo with cosine law
  double dS = sqrt(pow2(dL) + pow2(D_LS) - 2 * D_LS * dL * cosL);

  // calculate the angle TSR with sine law
  return degree(asin(dL * sin(acos(cosL)) / dS));
}

void loop() {
  const int TIMES = 3;

  // the sonars are not very stable, use the average of
  // multiple detections to move the servo smooth.
  double res = 0;
  for (int i = 0; i < TIMES; i++) {
    res += detect();
  }
  res /= TIMES;

  Serial.printf("Angle: %f\n", res);
  servo.write(round(180 - res));
}
