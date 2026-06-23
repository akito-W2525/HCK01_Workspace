// ==========================================
// 指揮者側メインシステム (conductor.ino)
// ==========================================

// --- ピン定義 ---
const int SENSOR_PIN = A0; 
const int POT_PIN = A1;    // 可変抵抗のアナログピン
const int MOTOR_IN1 = 9;   
const int MOTOR_IN2 = 10;  
const int SWITCH_PIN = 2;  // タクトスイッチ

// --- しきい値とノイズフィルタ設定 ---
const int THRESHOLD = 300;     
bool lastSensorState = false;  
unsigned long lastPulseTime = 0; 
bool pulseDetected = false;    

// --- システム状態管理 ---
bool isRunning = true;       // モータが回転中かどうか
bool lastSwitchState = HIGH; // スイッチの前の状態（PULLUPなのでHIGHが未押下）
const int BRAKE_TIME = 50;   // ★ブレーキをかける時間(ms)。テスト結果に合わせて変更してください

// --- 拍カウントとBPM計算用変数 ---
int slitCount = 0;                  
unsigned long previousSlitTime = 0; 
float currentBPM = 0.0;             
unsigned long currentDuration = 0;  
int beatcount  = 40;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; 

// ★決定済みゲインパラメータ
float Kp = 0.05;  // 比例
float Ki = 0.02;  // 積分
float Kd = 0.0;   // 微分
float integral = 0, previous_error = 0;


void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP); // 内部抵抗を有効化（配線をシンプルに）

  Serial.println("Target,Current,PWM"); 
  
  // 初期ブースト（モータの摩擦に負けないための勢いづけ）
  updatePWM(100); 
  delay(800); 
}

void loop() {
  // 1. スイッチの監視と状態切り替え
  handleSwitch();

  // 2. センサー読み取りとBPM計算・PID制御
  receivePulse();

  // 3. プロッタへのデータ出力
  if (pulseDetected) {
    pulseDetected = false; 
    
    // 回転中のみグラフを出力する
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

// --- スイッチ制御 ---
void handleSwitch() {
  // PULLUPなのでLOWが「押された」状態
  bool currentSwitchState = digitalRead(SWITCH_PIN);

  // ボタンが押された瞬間（HIGHからLOWに変わった時）を検知
  if (lastSwitchState == HIGH && currentSwitchState == LOW) {
    delay(30); // チャタリング防止（30ms待つ）
    
    if (digitalRead(SWITCH_PIN) == LOW) { // 本当に押されているか再確認
      if (isRunning) {
        // 回転中ならブレーキをかけて止める
        ReverseBrake();
        isRunning = false;
      } else {
        // 停止中ならシステムを再始動する
        slitCount = 0;
        integral = 0;
        previousSlitTime = 0;
        isRunning = true;
        updatePWM(100); // 再始動のブースト
        delay(800);
      }
    }
  }
  lastSwitchState = currentSwitchState;
}

// --- センサー検知とBPM計算 ---
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = false; 

  // ソフトウェア・ダブルチェック（ノイズ対策）
  if (sensorValue > THRESHOLD) {
    delayMicroseconds(50); 
    int confirmValue = analogRead(SENSOR_PIN);
    if (confirmValue > THRESHOLD) {
      currentSensorState = true; 
    }
  }

  // スリット検知（立ち上がりエッジ）
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

          // システム稼働中のみ、目標更新とPIDによる速度調整を行う
          if (isRunning) {
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

  // エンスト防止のためのベースパワー
  int basePWM = 65; 

  float output = basePWM + (Kp * error) + (Ki * integral) + (Kd * derivative);
  previous_error = error;

  int newPWM = (int)output; 
  updatePWM(newPWM);
  return newPWM;
}

// --- モータ出力制御（Uno R4 WiFi対応版） ---
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

// --- 逆相ブレーキ（Uno R4 WiFi対応版） ---
void ReverseBrake() {
  // 逆回転フルパワー
  analogWrite(MOTOR_IN1, 0);   
  analogWrite(MOTOR_IN2, 255); 
  
  delay(BRAKE_TIME); 
  
  // ショートブレーキで完全固定
  analogWrite(MOTOR_IN1, 255); 
  analogWrite(MOTOR_IN2, 255); 
  currentPWM = 0;
}