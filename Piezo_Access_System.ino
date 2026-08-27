/*
  Piezo Access-Control Keypad (Chorded Version)
  =============================================
 
*/

#include <Arduino.h>

// ---------------- USER CONFIG ----------------

const uint8_t PIEZO_PINS[] = {A0, A1, A2, A3};
const uint8_t NUM_PIEZOS = 4;

// The secret code as a sequence of CHORDS (number of simultaneous taps).
// Since we have 4 piezos, each number must be between 1 and 4!
const uint8_t SECRET_CODE[] = {1, 3, 2, 1};
const uint8_t CODE_LENGTH = sizeof(SECRET_CODE) / sizeof(SECRET_CODE[0]);


const int STRIKE_THRESHOLD = 50;
const unsigned long REFRACTORY_MS = 250;      // ignore ringing after a hit
const unsigned long CHORD_WINDOW_MS = 150;     // window to group simultaneous hits
const unsigned long ENTRY_TIMEOUT_MS = 4000;  // reset if paused mid-code

// Soft (timing) gate - calibrated from real enrollment data.
float TIMING_MEAN[CODE_LENGTH - 1]  = {1227.4, 908.6, 858.7};
float TIMING_STDEV[CODE_LENGTH - 1] = {413.0, 253.3, 229.5};
const float TOLERANCE_K = 1.5; // how many std-devs still count as "normal"

const uint8_t LED_GREEN = 2;
const uint8_t LED_RED   = 3;
const uint8_t BUZZER    = 9;

// ---------------- STATE ----------------

enum Mode { RUN, CALIBRATE, ENROLL };
Mode mode = RUN;

uint8_t enteredDigits[CODE_LENGTH];
unsigned long pressTimestamps[CODE_LENGTH];
uint8_t enteredCount = 0;

unsigned long lastPressTime[NUM_PIEZOS] = {0};

const uint8_t ENROLL_SAMPLES = 15;
float gapSums[CODE_LENGTH - 1];
float gapSumsSq[CODE_LENGTH - 1];
uint8_t enrollRound = 0;

// ---------------- SETUP / LOOP ----------------

void setup() {
  Serial.begin(9600);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  resetEntry();
  Serial.println(F("Ready. Send C=calibrate, E=enroll timing, R=run."));
}

void loop() {
  handleSerialCommands();

  if (mode == CALIBRATE) {
    runCalibrate();
    return;
  }

  uint8_t hitCount = detectStrikeCount();
  if (hitCount > 0) {
    registerPress(hitCount);
  }

  if (enteredCount > 0 &&
      millis() - pressTimestamps[enteredCount - 1] > ENTRY_TIMEOUT_MS) {
    Serial.println(F("Timeout - resetting entry."));
    resetEntry();
  }
}

// ---------------- INPUT HANDLING ----------------

uint8_t detectStrikeCount() {
  uint8_t count = 0;
  bool pressed[NUM_PIEZOS] = {false};
  unsigned long firstHitTime = 0;

  for (uint8_t i = 0; i < NUM_PIEZOS; i++) {
    if (analogRead(PIEZO_PINS[i]) >= STRIKE_THRESHOLD && millis() - lastPressTime[i] > REFRACTORY_MS) {
      pressed[i] = true;
      count++;
      firstHitTime = millis();
      lastPressTime[i] = millis();
    }
  }

  if (count == 0) return 0;

  while (millis() - firstHitTime < CHORD_WINDOW_MS) {
    for (uint8_t i = 0; i < NUM_PIEZOS; i++) {
      if (!pressed[i] && analogRead(PIEZO_PINS[i]) >= STRIKE_THRESHOLD && millis() - lastPressTime[i] > REFRACTORY_MS) {
        pressed[i] = true;
        count++;
        lastPressTime[i] = millis();
      }
    }
  }

  return count;
}

void registerPress(uint8_t digit) {
  unsigned long now = millis();

  if (mode == ENROLL) {
    enrollPress(digit, now);
    return;
  }

  enteredDigits[enteredCount] = digit;
  pressTimestamps[enteredCount] = now;
  enteredCount++;

  Serial.print(F("Chord Taps: "));
  Serial.println(digit);

  if (enteredCount == CODE_LENGTH) {
    evaluateEntry();
    resetEntry();
  }
}

void resetEntry() {
  enteredCount = 0;
}

// ---------------- HARD GATE + SOFT GATE ----------------

void evaluateEntry() {
  bool codeOK = true;
  for (uint8_t i = 0; i < CODE_LENGTH; i++) {
    if (enteredDigits[i] != SECRET_CODE[i]) { codeOK = false; break; }
  }

  if (!codeOK) {
    denyAccess();
    return;
  }

  bool timingOK = true;
  for (uint8_t i = 0; i < CODE_LENGTH - 1; i++) {
    float gap = (float)(pressTimestamps[i + 1] - pressTimestamps[i]);
    float dev = fabs(gap - TIMING_MEAN[i]);
    if (TIMING_STDEV[i] > 0 && dev > TOLERANCE_K * TIMING_STDEV[i]) {
      timingOK = false;
    }
  }

  grantAccess(timingOK);
}

void grantAccess(bool timingOK) {
  digitalWrite(LED_GREEN, HIGH);
  if (timingOK) {
    Serial.println(F("ACCESS GRANTED"));
  } else {
    Serial.println(F("ACCESS GRANTED - timing anomaly flagged"));
    tone(BUZZER, 1800, 150);
  }
  delay(1000);
  digitalWrite(LED_GREEN, LOW);
}

void denyAccess() {
  digitalWrite(LED_RED, HIGH);
  tone(BUZZER, 400, 400);
  Serial.println(F("ACCESS DENIED"));
  delay(1000);
  digitalWrite(LED_RED, LOW);
}

// ---------------- CALIBRATE MODE ----------------


void runCalibrate() {
  static int lastVals[NUM_PIEZOS] = {0, 0, 0, 0};
  int vals[NUM_PIEZOS];
  bool changed = false;

  for (uint8_t i = 0; i < NUM_PIEZOS; i++) {
    vals[i] = analogRead(PIEZO_PINS[i]);
    if (vals[i] != lastVals[i]) changed = true;
  }

  if (changed) {
    for (uint8_t i = 0; i < NUM_PIEZOS; i++) {
      Serial.print(vals[i]);
      Serial.print('\t');
      lastVals[i] = vals[i];
    }
    Serial.println();
  }

  delay(20);
}

// ---------------- ENROLL MODE ----------------

void enrollPress(uint8_t digit, unsigned long now) {
  enteredDigits[enteredCount] = digit;
  pressTimestamps[enteredCount] = now;
  enteredCount++;

  if (enteredCount < CODE_LENGTH) return;

  bool codeOK = true;
  for (uint8_t i = 0; i < CODE_LENGTH; i++) {
    if (enteredDigits[i] != SECRET_CODE[i]) codeOK = false;
  }

  if (codeOK) {
    for (uint8_t i = 0; i < CODE_LENGTH - 1; i++) {
      float gap = (float)(pressTimestamps[i + 1] - pressTimestamps[i]);
      gapSums[i] += gap;
      gapSumsSq[i] += gap * gap;
    }
    enrollRound++;
    Serial.print(F("Enroll sample "));
    Serial.print(enrollRound);
    Serial.print('/');
    Serial.println(ENROLL_SAMPLES);
  } else {
    Serial.println(F("Wrong code during enrollment - discarded, try again."));
  }

  resetEntry();

  if (enrollRound >= ENROLL_SAMPLES) {
    Serial.println(F("--- Enrollment done. Paste these into the sketch: ---"));

    Serial.print(F("float TIMING_MEAN[CODE_LENGTH - 1]  = {"));
    for (uint8_t i = 0; i < CODE_LENGTH - 1; i++) {
      float mean = gapSums[i] / ENROLL_SAMPLES;
      Serial.print(mean, 1);
      if (i < CODE_LENGTH - 2) Serial.print(F(", "));
    }
    Serial.println(F("};"));

    Serial.print(F("float TIMING_STDEV[CODE_LENGTH - 1] = {"));
    for (uint8_t i = 0; i < CODE_LENGTH - 1; i++) {
      float mean = gapSums[i] / ENROLL_SAMPLES;
      float variance = (gapSumsSq[i] / ENROLL_SAMPLES) - (mean * mean);
      float stdev = sqrt(variance > 0 ? variance : 0);
      Serial.print(stdev, 1);
      if (i < CODE_LENGTH - 2) Serial.print(F(", "));
    }
    Serial.println(F("};"));

    mode = RUN;
    enrollRound = 0;
    for (uint8_t i = 0; i < CODE_LENGTH - 1; i++) { gapSums[i] = 0; gapSumsSq[i] = 0; }
  }
}

// ---------------- SERIAL COMMANDS ----------------

void handleSerialCommands() {
  if (!Serial.available()) return;
  char c = Serial.read();

  if (c == 'C' || c == 'c') {
    mode = CALIBRATE;
    Serial.println(F("CALIBRATE: raw values A0 A1 A2 A3, tab-separated."));
  } else if (c == 'E' || c == 'e') {
    mode = ENROLL;
    enrollRound = 0;
    resetEntry();
    for (uint8_t i = 0; i < CODE_LENGTH - 1; i++) { gapSums[i] = 0; gapSumsSq[i] = 0; }
    Serial.println(F("ENROLL: enter the correct code naturally, repeatedly."));
  } else if (c == 'R' || c == 'r') {
    mode = RUN;
    resetEntry();
    Serial.println(F("RUN mode."));
  }
}
