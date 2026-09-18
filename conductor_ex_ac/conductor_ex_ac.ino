// ==========================================
// 指揮者側メインシステム (conductor_ex_ac.ino) - 輪唱制御・LEDマトリクスBPM表示搭載 (加速度センサ版)
// ==========================================
#include "Arduino_LED_Matrix.h" // LEDマトリクスライブラリ

ArduinoLEDMatrix matrix; //　インスタンス生成

// --- ピン定義 ---
const int SENSOR_PIN = A0; 
const int MOTOR_IN1 = 9;
const int MOTOR_IN2 = 10;  
const int SWITCH_PIN = 2;  

// 楽器制御用の電源出力ピン
const int PIN_PERC   = 3; // 打楽器（リズム）
const int PIN_MELO_1 = 4; // 主旋律1
const int PIN_MELO_2 = 5; // 主旋律2
const int PIN_MELO_3 = 6; // 主旋律3

// --- 輪唱・演奏タイミング設定 ---
// 2小節でスリットを何個通過するかを設定
const int SLITS_PER_MEASURE = 360; 

// モータが安定するまで演奏を待つ「空回り（助走）」のスリット数
const int COUNT_IN_SLITS = 40;
unsigned long totalSlitCount = 0; // システム起動からの総スリット通過数

// --- しきい値とノイズフィルタ設定 ---
const int THRESHOLD = 300;     
bool lastSensorState = false;
unsigned long lastPulseTime = 0; 
bool pulseDetected = false;    

// --- システム状態管理 ---
bool isRunning = true;       
bool lastSwitchState = HIGH;
const int BRAKE_TIME = 50;   

// --- 拍カウントとBPM計算用変数 ---
int slitCount = 0;                  
unsigned long previousSlitTime = 0;
float currentBPM = 0.0;             
unsigned long currentDuration = 0;  
int beatcount  = 45;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; 

float Kp = 0.05;  
float Ki = 0.02;
float Kd = 0.0;   
float integral = 0, previous_error = 0;

// --- 加速度センサ変数 ---
const int ACCEL_PIN = A2; // 加速度センサーのX軸ピン 
const int ACCEL_THRESHOLD = 590;         
const unsigned long SHAKE_COOLDOWN = 200; // 連続反応を防ぐクールダウン(ms)
const unsigned long DECAY_INTERVAL = 100; // 減衰処理を行う間隔(ms)

unsigned long lastShakeTime = 0;         // 最後に振りを検知した時間
unsigned long lastDecayTime = 0;         // 最後に減衰処理を行った時間
float currentTargetBPM = 120.0;          // 現在の目標BPM

// --- LED表示用タイマー変数 ---
unsigned long lastLEDUpdateTime = 0;
const unsigned long LED_UPDATE_INTERVAL = 200; // LEDの更新間隔 (200ms)

// --- 3x5 ドットフォント定義 (0〜9) ---
const uint8_t font3x5[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // 楽器制御ピンの初期化と全消灯
  pinMode(PIN_PERC, OUTPUT);
  pinMode(PIN_MELO_1, OUTPUT);
  pinMode(PIN_MELO_2, OUTPUT);
  pinMode(PIN_MELO_3, OUTPUT);
  turnOffAllInstruments();

  // LEDマトリクスの初期化
  matrix.begin();

  Serial.println("Target,Current,PWM"); 
  
  updatePWM(250); 
  delay(1000); 
}

void loop() {
  handleSwitch();
  receivePulse();

  // --- LEDマトリクスの更新 ---
  unsigned long now = millis();
  if (now - lastLEDUpdateTime >= LED_UPDATE_INTERVAL) {
    lastLEDUpdateTime = now;
    if (isRunning) {
      displayBPM(currentBPM); // 走行中は現在のBPMを表示
    } else {
      displayStopPattern();   // 停止時は「---」を表示
    }
  }

  if (isRunning) {
    targetBPM = readTargetBPM();
  }

  if (pulseDetected) {
    pulseDetected = false;
    
    if (isRunning) {
      Serial.print("Target:");
      Serial.print(targetBPM);
      Serial.print(",");
      Serial.print("Current:");
      Serial.print(currentBPM);
      Serial.print(",");
      Serial.print("PWM:");
      Serial.println(currentPWM);

      if (slitCount >= beatcount) {
        slitCount = 0;
      }
    }
  }
}

// ==========================================
// サブ関数群
// ==========================================

// --- BPM数値を12x8バッファにマッピングして表示する関数 ---
void displayBPM(float bpm) {
  int bpmInt = (int)bpm;
  if (bpmInt < 0 || bpmInt > 999) return;

  // 各桁の分解
  int digits[3];
  digits[0] = (bpmInt / 100) % 10; // 百の位
  digits[1] = (bpmInt / 10) % 10;  // 十の位
  digits[2] = bpmInt % 10;         // 一の位

  uint8_t frame[8][12] = {0};

  // 3つの数字を12x8マトリクスに配置（行1〜5に描画、列0~2, 4~6, 8~10に割り当て）
  for (int d = 0; d < 3; d++) {
    int digit = digits[d];
    int colOffset = d * 4; // 桁ごとのX軸オフセット (0, 4, 8)

    // 百の位が0の場合は非表示（ゼロサプレス）にして見やすくする
    if (d == 0 && digit == 0) continue;

    for (int row = 0; row < 5; row++) {
      uint8_t rowData = font3x5[digit][row];
      for (int col = 0; col < 3; col++) {
        // ビットシフトで各ドットのON/OFFを判定
        if ((rowData >> (2 - col)) & 1) {
          frame[row + 1][colOffset + col] = 1; // 上下に1マス余白を持たせて配置
        }
      }
    }
  }
  matrix.renderBitmap(frame, 8, 12);
}

// --- 停止中の表示パターン (中央に3本の横線「---」) ---
void displayStopPattern() {
  uint8_t stopFrame[8][12] = {
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,0,0,1,1,0,0,1,1,0}, // 中央にダッシュを並べる
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
  };
  matrix.renderBitmap(stopFrame, 8, 12);
}

// --- 全楽器のセンサー電源を落とす（演奏停止） ---
void turnOffAllInstruments() {
  digitalWrite(PIN_PERC, LOW);
  digitalWrite(PIN_MELO_1, LOW);
  digitalWrite(PIN_MELO_2, LOW);
  digitalWrite(PIN_MELO_3, LOW);
}

// --- スイッチ制御 ---
void handleSwitch() {
  bool currentSwitchState = digitalRead(SWITCH_PIN);
  if (lastSwitchState == HIGH && currentSwitchState == LOW) {
    delay(30);
    if (digitalRead(SWITCH_PIN) == LOW) { 
      if (isRunning) {
        // 【停止時】モータにブレーキをかけ、全楽器のLEDを消して即座に演奏を止める
        ReverseBrake();
        turnOffAllInstruments();
        isRunning = false;
        Serial.println("\n>>> 全演奏 停止 <<<");
      } else {
        // 【再始動】カウントをリセットし、助走（LEDは消えたまま）を開始する
        slitCount = 0;
        totalSlitCount = 0; 
        integral = 0;
        previousSlitTime = 0;
        isRunning = true;
        updatePWM(100); 
        delay(800);
      }
    }
  }
  lastSwitchState = currentSwitchState;
}

// --- センサー検知とBPM計算・輪唱キュー出し ---
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = false;
  if (sensorValue > THRESHOLD) {
    delayMicroseconds(50); 
    int confirmValue = analogRead(SENSOR_PIN);
    if (confirmValue > THRESHOLD) {
      currentSensorState = true;
    }
  }

  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = micros();
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;
      if (previousSlitTime == 0) {
        previousSlitTime = currentTime;
      } 
      else {
        unsigned long duration = currentTime - previousSlitTime;
        previousSlitTime = currentTime;

        if (duration > 0 && duration < 2000000) {
          currentDuration = duration;
          currentBPM = ( 60000000.0 / ((float)duration * beatcount * 2));
          slitCount++;
          
          // --- 輪唱のタイミング制御（キュー出し） ---
          if (isRunning) {
            totalSlitCount++; // 起動してからの総スリット数をカウント

            // 1. カウントイン完了：まずは「打楽器」だけがスタート
            if (totalSlitCount == COUNT_IN_SLITS) {
              digitalWrite(PIN_PERC, HIGH);
            }
            // 2. 打楽器から1小節経過：「主旋律1」がスタート
            else if (totalSlitCount == COUNT_IN_SLITS + SLITS_PER_MEASURE) {
              digitalWrite(PIN_MELO_1, HIGH);
            }
            // 3. 主旋律1から2小節遅れ（打楽器からは3小節）：「主旋律2」がスタート
            else if (totalSlitCount == COUNT_IN_SLITS + (SLITS_PER_MEASURE * 3)) {
              digitalWrite(PIN_MELO_2, HIGH);
            }
            // 4. 主旋律1から4小節遅れ（打楽器からは5小節）：「主旋律3」がスタート
            else if (totalSlitCount == COUNT_IN_SLITS + (SLITS_PER_MEASURE * 5)) {
              digitalWrite(PIN_MELO_3, HIGH);
            }

            // PIDの更新
            updatePID(targetBPM, currentBPM);
          }
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}

// --- 目標BPM読み取り --- 加速度センサ
float readTargetBPM() {
  unsigned long now = millis();
  
  // 1. 加速度センサの値を読み取る
  int accelValue = analogRead(ACCEL_PIN);

  // 2. 振りの検知（しきい値を超え、かつクールダウンが経過しているか）
  if (accelValue > ACCEL_THRESHOLD && (now - lastShakeTime > SHAKE_COOLDOWN)) {
    currentTargetBPM += 5.0; // 1振りにつきBPMを5上げる
    lastShakeTime = now;
  }

  // 3. 時間経過による減衰
  if (now - lastDecayTime > DECAY_INTERVAL) {
    currentTargetBPM -= 1;
    lastDecayTime = now;
  }

  // 4. BPMの範囲を制限する
  if (currentTargetBPM > 160.0) {
    currentTargetBPM = 160.0;
  } else if (currentTargetBPM < 80.0) {
    currentTargetBPM = 80.0;
  }

  return currentTargetBPM;
}

// --- PID演算 ---
int updatePID(float targetBPM, float currentBPM) {
  float error = targetBPM - currentBPM;
  integral += error;
  integral = constrain(integral, -10000, 10000);

  float derivative = error - previous_error;

  int basePWM = 65;
  float output = basePWM + (Kp * error) + (Ki * integral) + (Kd * derivative);
  previous_error = error;
  int newPWM = (int)output; 
  updatePWM(newPWM);
  return newPWM;
}

// --- モータ出力制御 ---
void updatePWM(int pwmValue) {
  if (pwmValue <= 0) {
    analogWrite(MOTOR_IN1, 255);
    analogWrite(MOTOR_IN2, 255);
    currentPWM = 0;
  } 
  else {
    pwmValue = constrain(pwmValue, 0, 255);
    analogWrite(MOTOR_IN1, pwmValue);
    analogWrite(MOTOR_IN2, 0); 
    currentPWM = pwmValue;
  }
}

// --- 逆相ブレーキ ---
void ReverseBrake() {
  analogWrite(MOTOR_IN1, 0);   
  analogWrite(MOTOR_IN2, 255); 
  
  delay(BRAKE_TIME); 
  
  analogWrite(MOTOR_IN1, 255);
  analogWrite(MOTOR_IN2, 255); 
  currentPWM = 0;
}