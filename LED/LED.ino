#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

uint8_t tadpole1[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,1,1,1,1,0,0,0,0,0,0,0},
  {1,0,0,0,0,1,1,0,0,0,0,0},
  {1,0,1,0,0,0,0,1,1,1,1,1},
  {1,0,0,0,0,0,0,1,1,0,0,0},
  {1,0,0,0,0,0,1,1,0,0,0,0},
  {0,1,1,1,1,1,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t tadpole2[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,1,1,1,1,0,0,0,0,0,0,0},
  {1,0,0,0,0,1,1,0,0,0,0,0},
  {1,0,1,0,0,0,0,1,1,0,0,0},
  {1,0,0,0,0,0,0,1,1,1,1,1},
  {1,0,0,0,0,0,1,1,0,0,0,0},
  {0,1,1,1,1,1,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t frog1[8][12] = {
  {0,0,1,1,0,0,0,0,1,1,0,0},
  {0,1,0,0,1,0,0,1,0,0,1,0},
  {0,1,0,0,1,1,1,1,0,0,1,0},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,1,1,1,1,1,0,0,1},
  {1,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,0,1,0,0,0,0,1,0,0,1},
  {0,1,0,0,1,1,1,1,0,0,1,0}
};

uint8_t frog2[8][12] = {
  {0,0,1,1,0,0,0,0,1,1,0,0},
  {0,1,0,0,1,0,0,1,0,0,1,0},
  {0,1,0,0,1,1,1,1,0,0,1,0},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,1,1,1,1,1,0,0,1},
  {0,1,0,0,0,0,0,0,0,0,1,0}
};

int mode = 1;
bool toggle = true;
unsigned long lastSwitch = 0;
const unsigned long interval = 500;

void setup() {
  Serial.begin(9600);
  matrix.begin();
  matrix.renderBitmap(tadpole1, 8, 12);
  lastSwitch = millis();
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '1') {
      mode = 1;
      toggle = true;
      matrix.renderBitmap(tadpole1, 8, 12);
      lastSwitch = millis();
      Serial.println("おたまじゃくしのアニメーションを開始しました");
    } 
    else if (c == '2') {
      mode = 2;
      toggle = true;
      matrix.renderBitmap(frog1, 8, 12);
      lastSwitch = millis();
      Serial.println("カエルのアニメーションを開始しました");
    }
  }

  if (millis() - lastSwitch >= interval) {
    lastSwitch = millis();
    toggle = !toggle;

    if (mode == 1) {
      if (toggle) {
        matrix.renderBitmap(tadpole1, 8, 12);
      } else {
        matrix.renderBitmap(tadpole2, 8, 12);
      }
    } else if (mode == 2) {
      if (toggle) {
        matrix.renderBitmap(frog1, 8, 12);
      } else {
        matrix.renderBitmap(frog2, 8, 12);
      }
    }
  }
}