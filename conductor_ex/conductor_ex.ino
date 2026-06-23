// ==========================================
// 指揮者側メインシステム (conductor_ex.ino) - 輪唱制御搭載
// ==========================================

// --- ピン定義 ---
const int SENSOR_PIN = A0; 
const int POT_PIN = A1;    
const int MOTOR_IN1 = 9;   
const int MOTOR_IN2 = 10;  
const int SWITCH_PIN = 2;  

// ★追加：楽器制御用の電源出力ピン
const int PIN_PERC   = 3; // 打楽器（リズム）
const int PIN_MELO_1 = 4; // 主旋律1
const int PIN_MELO_2 = 5; // 主旋律2
const int PIN_MELO_3 = 6; // 主旋律3

// --- 輪唱・演奏タイミング設定 ---
// 【重要】1小節でスリットを何個通過するかを設定
const int SLITS_PER_MEASURE = 320; 

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
int beatcount  = 40;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; 

float Kp = 0.05;  
float Ki = 0.02;  
float Kd = 0.0;   
float integral = 0, previous_error = 0;


void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // ★追加：楽器制御ピンの初期化と全消灯
  pinMode(PIN_PERC, OUTPUT);
  pinMode(PIN_MELO_1, OUTPUT);
  pinMode(PIN_MELO_2, OUTPUT);
  pinMode(PIN_MELO_3, OUTPUT);
  turnOffAllInstruments();

  Serial.println("Target,Current,PWM"); 
  
  updatePWM(100); 
  delay(800); 
}

void loop() {
  handleSwitch();
  receivePulse();

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

// --- ★追加：全楽器のセンサー電源を落とす（演奏停止） ---
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

// --- センサー検知とBPM計算・★輪唱キュー出し ---
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

          // --- ★ここからが輪唱のタイミング制御（キュー出し） ---
          if (isRunning) {
            totalSlitCount++; // 起動してからの総スリット数をカウント

            // 1. カウントイン完了：打楽器と主旋律1がスタート（LEDオン）
            if (totalSlitCount == COUNT_IN_SLITS) {
              digitalWrite(PIN_PERC, HIGH);
              digitalWrite(PIN_MELO_1, HIGH);
            }
            // 2. 2小節遅れ：主旋律2がスタート
            else if (totalSlitCount == COUNT_IN_SLITS + (SLITS_PER_MEASURE * 2)) {
              digitalWrite(PIN_MELO_2, HIGH);
            }
            // 3. さらに2小節（計4小節）遅れ：主旋律3がスタート
            else if (totalSlitCount == COUNT_IN_SLITS + (SLITS_PER_MEASURE * 4)) {
              digitalWrite(PIN_MELO_3, HIGH);
            }

            // PIDによる速度調整
            targetBPM = readTargetBPM();
            updatePID(targetBPM, currentBPM);
          }
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}

// --- 目標BPM読み取り ---
float readTargetBPM() {
  int rawValue = analogRead(POT_PIN); 
  long mappedBPM = map(rawValue, 0, 1023, 80, 160);
  return (float)mappedBPM;
}

// --- PID演算 ---
int updatePID(float targetBPM, float currentBPM) {
  float error = targetBPM - currentBPM;
  integral += error;
  integral = constrain(integral, -2000, 2000);

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