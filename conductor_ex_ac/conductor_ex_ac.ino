// ==========================================
// 指揮者側メインシステム (conductor_ex.ino) - 輪唱制御搭載
// ==========================================

// --- ピン定義 ---
const int SENSOR_PIN = A0; 
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

//加速度センサ変数
const int ACCEL_PIN = A2;                // 加速度センサーのX軸ピン 
const int ACCEL_THRESHOLD = 650;         // 振りを検知するしきい値（通常時は約512）
const unsigned long SHAKE_COOLDOWN = 200; // 連続反応を防ぐクールダウン(ms)
const unsigned long DECAY_INTERVAL = 100; // 減衰処理を行う間隔(ms)

unsigned long lastShakeTime = 0;         // 最後に振りを検知した時間
unsigned long lastDecayTime = 0;         // 最後に減衰処理を行った時間
float currentTargetBPM = 120.0;          // 現在の目標BPM


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

            // ★修正：目標BPMの読み取りは削除し、PIDの更新だけを行う
            updatePID(targetBPM, currentBPM);
          }
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}

// --- 目標BPM読み取り ---　加速度センサ
float readTargetBPM() {
  unsigned long now = millis();

  // 1. 加速度センサの値を読み取る
  int accelValue = analogRead(ACCEL_PIN);

  // 2. 振りの検知（しきい値を超え、かつクールダウンが経過しているか）
  // 勢いよく振って値が650を超えたら「1振り」とみなす
  if (accelValue > ACCEL_THRESHOLD && (now - lastShakeTime > SHAKE_COOLDOWN)) {
    currentTargetBPM += 5.0; // 1振りにつきBPMを5上げる
    lastShakeTime = now;
  }

  // 3. 時間経過による減衰（例：100msごとに -0.3 される）
  if (now - lastDecayTime > DECAY_INTERVAL) {
    currentTargetBPM -= 0.3; 
    lastDecayTime = now;
  }

  // 4. BPMの範囲を制限する（80〜160の間に押し込める）
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