// --- ピン定義 ---
const int SENSOR_PIN = A0; 
const int POT_PIN = A1;    // 可変抵抗のアナログピン
const int MOTOR_IN1 = 9;   
const int MOTOR_IN2 = 10;  

// --- しきい値と状態管理 ---
const int THRESHOLD = 300;     
bool lastSensorState = false;  
unsigned long lastPulseTime = 0; 
bool pulseDetected = false;    

// --- 拍カウントとBPM計算用変数 ---
int slitCount = 0;                  
unsigned long previousSlitTime = 0; 
float currentBPM = 0.0;             
unsigned long currentDuration = 0;  
int beatcount  = 40;

// --- ★ブレーキテスト用変数 ---
bool isMotorRunning = true;       // モータが回転中か
bool isBraking = false;           // ブレーキテスト中か
unsigned long brakeStartTime = 0; // ブレーキ開始時刻
int extraSlitCount = 0;           // ブレーキ後の余分な拍

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; 
// ★決定したゲインパラメータ（そのまま維持）
float Kp = 0.05;  // 比例
float Ki = 0.02;  // 積分
float Kd = 0;     // 微分
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.println("=====================================");
  Serial.println("  Reverse Brake Test Ready.");
  Serial.println("  [g] : モータ回転スタート");
  Serial.println("  [s] : 逆相ブレーキ作動 (Stop)");
  Serial.println("=====================================");
  
  // 初期ベースパワーで回り始める（ブースト）
  updatePWM(100); 
  delay(800); // 最初の0.5秒間(プログラム上は0.8秒)はフルパワーで勢いをつける！
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. シリアルモニタからの操作を受付
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    // 'g' が送信されたらスタート
    if (cmd == 'g' || cmd == 'G') {
      Serial.println("\n>>> モータ回転スタート <<<");
      slitCount = 0;
      extraSlitCount = 0;
      integral = 0; // PIDの積分をリセット
      previousSlitTime = 0; // 時間計測リセット
      isMotorRunning = true;
      isBraking = false;
      
      // 再始動時のブースト
      updatePWM(100); 
      delay(800);
    } 
    // 's' が送信されたらブレーキテスト開始
    else if (cmd == 's' || cmd == 'S') {
      if (isMotorRunning) {
        Serial.println("\n>>> 逆相ブレーキ作動！ <<<");
        ReverseBrake(); 
        isMotorRunning = false;
        isBraking = true;
        brakeStartTime = millis(); 
      }
    }
  }

  // 3. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; 
    
    if (isMotorRunning) {
      // 回転中の表示（安定状態の確認用）
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
    else if (isBraking) {
      // ブレーキ作動中の検知（＝余分な拍）
      extraSlitCount++;
      Serial.print("【警告】余分なスリットを検知: 計 ");
      Serial.print(extraSlitCount);
      Serial.println(" 回");
    }
  }

  // 4. ブレーキ作動から2秒経過したら結果を発表
  if (isBraking && (millis() - brakeStartTime > 2000)) {
    isBraking = false; 
    
    Serial.println("\n--- ブレーキテスト結果 ---");
    if (extraSlitCount == 0) {
      Serial.println("【合格】余分な拍の検知は 0 回でした！完璧に停止しています．");
    } else {
      Serial.print("【不合格】余分な拍が ");
      Serial.print(extraSlitCount);
      Serial.println(" 回発生しました．慣性で空回りしています．");
      Serial.println(" -> ReverseBrake() 内の delay(50) の数値を調整してください．");
    }
    Serial.println("=====================================");
    Serial.println("再度テストする場合は [g] を送信してください．");
  }
}

// ==========================================
// 1. receivePulse() : アナログ読み取りとエッジ検出（デジタルフィルタ維持）
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = false; // とりあえず初期値はfalseにしておく

  // 究極のソフトウェア・ダブルチェック
  if (sensorValue > THRESHOLD) {
    // しきい値を超えた！でも電気ノイズ（火花）かもしれないので...
    // 50マイクロ秒だけ待って「本当にまだ光が当たっているか」を再確認します。
    delayMicroseconds(50); 
    
    int confirmValue = analogRead(SENSOR_PIN);
    if (confirmValue > THRESHOLD) {
      currentSensorState = true; // 50us後も超えていれば、本物のスリットだと認める
    }
  }

  // 立ち上がりエッジの検出
  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = micros(); 
    
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;

      // 最初の1回目の検知は、時間を記録するだけでスキップする
      if (previousSlitTime == 0) {
        previousSlitTime = currentTime;
      } 
      else {
        unsigned long duration = currentTime - previousSlitTime;
        previousSlitTime = currentTime;

        if (duration > 0 && duration < 2000000) {
          currentDuration = duration; 
          
          // BPMの計算 (8分音符基準)
          currentBPM = ( 60000000.0 / ((float)duration * beatcount * 2));
          slitCount++;

          // 毎スリットごとに可変抵抗を読み取って目標BPMを更新する
          targetBPM = readTargetBPM();

          // モータ回転中のみPIDを呼び出し、速度を自動調整
          if (isMotorRunning) {
            updatePID(targetBPM, currentBPM);
          }
        }
      }
    }
  }
  
  lastSensorState = currentSensorState;
}

// ==========================================
// 2. updatePWM() : 自動ブレーキ機能付き
// ==========================================
void updatePWM(int pwmValue) {
  if (pwmValue <= 0) {
    // digitalWriteの代わりに、analogWriteの255(HIGH)を使用する
    analogWrite(MOTOR_IN1, 255);
    analogWrite(MOTOR_IN2, 255);
    currentPWM = 0;
  } 
  else {
    pwmValue = constrain(pwmValue, 0, 255);
    analogWrite(MOTOR_IN1, pwmValue);
    // digitalWriteの代わりに、analogWriteの0(LOW)を使用する
    analogWrite(MOTOR_IN2, 0); 
    currentPWM = pwmValue;
  }
}

// ==========================================
// 3. updatePID() : ベースパワー追加版（そのまま維持）
// ==========================================
int updatePID(float targetBPM, float currentBPM) {
  float error = targetBPM - currentBPM;
  integral += error;
  
  integral = constrain(integral, -2000, 2000);

  float derivative = error - previous_error;

  int basePWM = 40; 

  float output = basePWM + (Kp * error) + (Ki * integral) + (Kd * derivative);
  previous_error = error;

  int newPWM = (int)output; 

  updatePWM(newPWM);
  return newPWM;
}

// ==========================================
// 4. ReverseBrake() : 停止時ブレーキ関数（Uno R4 WiFi完全対応版）
// ==========================================
void ReverseBrake() {
  // 逆相ブレーキ (フルパワーで逆回転)
  analogWrite(MOTOR_IN1, 0);   // LOWの代わり
  analogWrite(MOTOR_IN2, 255); // HIGHの代わり
  
  // ★ここの数値(ms)をテスト結果に合わせて調整します
  delay(50); 
  
  // ショートブレーキ (ガッチリ固定する)
  analogWrite(MOTOR_IN1, 255); // HIGHの代わり
  analogWrite(MOTOR_IN2, 255); // HIGHの代わり
  currentPWM = 0;
}

// ==========================================
// 5. readTargetBPM() : 目標BPM読み取り関数（そのまま維持）
// ==========================================
float readTargetBPM() {
  int rawValue = analogRead(POT_PIN); // 0 〜 1023 を取得
  
  // 0〜1023のアナログ値を、BPM 100 〜 160 の範囲に変換する
  long mappedBPM = map(rawValue, 0, 1023, 100, 160);
  
  return (float)mappedBPM;
}