/*
  GRBL-like single-file controller for 28BYJ-48 steppers (ULN2003)
  - No Stepper.h or AccelStepper.h
  - Split into main .ino and pins.h for clarity
  - pins.h defines X1..X4, Y1..Y4, Z1..Z4 pin assignments
*/

#include "pins.h"

#define BAUDRATE 115200

const uint8_t X_PINS[4] = {X1, X2, X3, X4};
const uint8_t Y_PINS[4] = {Y1, Y2, Y3, Y4};
const uint8_t Z_PINS[4] = {Z1, Z2, Z3, Z4};

const float STEPS_PER_MM_X = 100.0;
const float STEPS_PER_MM_Y = 100.0;
const float STEPS_PER_MM_Z = 100.0;

const float MAX_FEEDRATE = 2000.0;
const float MIN_FEEDRATE = 1.0;

const uint8_t SEQ_LEN = 8;
const uint8_t SEQ[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

volatile long currentStepsX = 0;
volatile long currentStepsY = 0;
volatile long currentStepsZ = 0;

float positionX = 0.0;
float positionY = 0.0;
float positionZ = 0.0;

bool relativeMode = false;

struct Move {
  bool active;
  long targetStepsX;
  long targetStepsY;
  long targetStepsZ;
  long deltaX;
  long deltaY;
  long deltaZ;
  long absDeltaX;
  long absDeltaY;
  long absDeltaZ;
  long stepsTotal;
  float feed_mm_per_min;
} moveCmd;

String serialLine = "";

uint8_t idxX = 0;
uint8_t idxY = 0;
uint8_t idxZ = 0;

inline float stepsToMM(long steps, float stepsPerMM) { return (float)steps / stepsPerMM; }
inline long mmToSteps(float mm, float stepsPerMM) { return lround(mm * stepsPerMM); }

void setupPins() {
  for (int i=0;i<4;i++) {
    pinMode(X_PINS[i], OUTPUT);
    pinMode(Y_PINS[i], OUTPUT);
    pinMode(Z_PINS[i], OUTPUT);
    digitalWrite(X_PINS[i], LOW);
    digitalWrite(Y_PINS[i], LOW);
    digitalWrite(Z_PINS[i], LOW);
  }
}

void setup() {
  Serial.begin(BAUDRATE);
  setupPins();
  moveCmd.active = false;
  delay(50);
  Serial.println("CustomGRBL-28BYJ v1.2");
  Serial.println("ok");
}

void stepMotor(const uint8_t pins[4], uint8_t &idx, int dir) {
  idx = (idx + (dir > 0 ? 1 : -1) + SEQ_LEN) % SEQ_LEN;
  for (int i=0; i<4; i++) {
    digitalWrite(pins[i], SEQ[idx][i]);
  }
}

void executeMove() {
  long dx = moveCmd.deltaX;
  long dy = moveCmd.deltaY;
  long dz = moveCmd.deltaZ;

  long absDx = abs(dx);
  long absDy = abs(dy);
  long absDz = abs(dz);

  long steps = max(absDx, max(absDy, absDz));
  if (steps == 0) return;

  float feed = constrain(moveCmd.feed_mm_per_min, MIN_FEEDRATE, MAX_FEEDRATE);
  float delayPerStep = (60000000.0 / (feed * steps)); // µs per step (rough)

  long errX = 0, errY = 0, errZ = 0;

  for (long i = 0; i < steps; i++) {
    if (absDx > 0) {
      errX += absDx;
      if (errX >= steps) {
        errX -= steps;
        stepMotor(X_PINS, idxX, dx > 0 ? 1 : -1);
        currentStepsX += (dx > 0 ? 1 : -1);
      }
    }
    if (absDy > 0) {
      errY += absDy;
      if (errY >= steps) {
        errY -= steps;
        stepMotor(Y_PINS, idxY, dy > 0 ? 1 : -1);
        currentStepsY += (dy > 0 ? 1 : -1);
      }
    }
    if (absDz > 0) {
      errZ += absDz;
      if (errZ >= steps) {
        errZ -= steps;
        stepMotor(Z_PINS, idxZ, dz > 0 ? 1 : -1);
        currentStepsZ += (dz > 0 ? 1 : -1);
      }
    }
    delayMicroseconds(delayPerStep);
  }

  positionX = stepsToMM(currentStepsX, STEPS_PER_MM_X);
  positionY = stepsToMM(currentStepsY, STEPS_PER_MM_Y);
  positionZ = stepsToMM(currentStepsZ, STEPS_PER_MM_Z);
}

void processCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.startsWith("G0") || line.startsWith("G1")) {
    float x = positionX;
    float y = positionY;
    float z = positionZ;
    float f = moveCmd.feed_mm_per_min > 0 ? moveCmd.feed_mm_per_min : 300;

    int idxX = line.indexOf('X');
    if (idxX >= 0) x = (relativeMode ? x : 0) + line.substring(idxX+1).toFloat();
    int idxY = line.indexOf('Y');
    if (idxY >= 0) y = (relativeMode ? y : 0) + line.substring(idxY+1).toFloat();
    int idxZ = line.indexOf('Z');
    if (idxZ >= 0) z = (relativeMode ? z : 0) + line.substring(idxZ+1).toFloat();
    int idxF = line.indexOf('F');
    if (idxF >= 0) f = line.substring(idxF+1).toFloat();

    moveCmd.deltaX = mmToSteps(x, STEPS_PER_MM_X) - currentStepsX;
    moveCmd.deltaY = mmToSteps(y, STEPS_PER_MM_Y) - currentStepsY;
    moveCmd.deltaZ = mmToSteps(z, STEPS_PER_MM_Z) - currentStepsZ;
    moveCmd.feed_mm_per_min = f;

    executeMove();
    Serial.println("ok");
  } else if (line.startsWith("G90")) {
    relativeMode = false;
    Serial.println("ok");
  } else if (line.startsWith("G91")) {
    relativeMode = true;
    Serial.println("ok");
  } else if (line.startsWith("G92")) {
    int idxX = line.indexOf('X');
    int idxY = line.indexOf('Y');
    int idxZ = line.indexOf('Z');

    if (idxX >= 0) {
      float newX = line.substring(idxX+1).toFloat();
      currentStepsX = mmToSteps(newX, STEPS_PER_MM_X);
      positionX = newX;
    }
    if (idxY >= 0) {
      float newY = line.substring(idxY+1).toFloat();
      currentStepsY = mmToSteps(newY, STEPS_PER_MM_Y);
      positionY = newY;
    }
    if (idxZ >= 0) {
      float newZ = line.substring(idxZ+1).toFloat();
      currentStepsZ = mmToSteps(newZ, STEPS_PER_MM_Z);
      positionZ = newZ;
    }
    Serial.println("ok");
  } else if (line.startsWith("M114")) {
    Serial.print("X:"); Serial.print(positionX, 3);
    Serial.print(" Y:"); Serial.print(positionY, 3);
    Serial.print(" Z:"); Serial.print(positionZ, 3);
    Serial.println();
    Serial.println("ok");
  } else {
    Serial.println("ok");
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialLine.length() > 0) {
        processCommand(serialLine);
        serialLine = "";
      }
    } else {
      serialLine += c;
    }
  }
}
