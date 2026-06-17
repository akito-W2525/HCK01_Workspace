// --- ピン定義 ---
const int SENSOR_PIN = A0; 
const int POT_PIN = A1;    // ★追加：可変抵抗のアナログピン
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

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; 
// ★決定したゲインパラメータ
float Kp = 0.3;  // 比例
float Ki = 0.1;  // 積分
float Kd = 0.01; // 微分
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  // プロッタのノイズになるため、初期のテキストは最小限にします
  Serial.println("Target,Current,PWM"); 
  
  // 初期ベースパワーで回り始める
  updatePWM(45); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // ※シリアルからの文字受信（1,2,3）は削除し、可変抵抗に任せます

  // 2. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; 
    
    // 【シリアルプロッタ用の表示形式】
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

// ==========================================
// 1. receivePulse() : アナログ読み取りとエッジ検出
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = (sensorValue > THRESHOLD);

  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = micros(); 
    
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;

      unsigned long duration = currentTime - previousSlitTime;
      previousSlitTime = currentTime;

      if (duration > 0 && duration < 2000000) {
        currentDuration = duration; 
        
        // BPMの計算 (8分音符基準)
        currentBPM = ( 60000000.0 / ((float)duration * beatcount * 2));
        slitCount++;

        // ★追加：毎スリットごとに可変抵抗を読み取って目標BPMを更新する
        targetBPM = readTargetBPM();

        // 毎スリットPIDを呼び出し、モーター速度を自動調整
        updatePID(targetBPM, currentBPM);
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
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, HIGH);
    currentPWM = 0;
  } 
  else {
    pwmValue = constrain(pwmValue, 0, 255);
    analogWrite(MOTOR_IN1, pwmValue);
    digitalWrite(MOTOR_IN2, LOW);
    currentPWM = pwmValue;
  }
}

// ==========================================
// 3. updatePID() : ベースパワー追加版
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
// 4. ReverseBrake() : 停止時ブレーキ関数
// ==========================================
void ReverseBrake() {
  digitalWrite(MOTOR_IN1, LOW);
  analogWrite(MOTOR_IN2, 255);
  delay(50); 
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, HIGH);
  currentPWM = 0;
}

// ==========================================
// ★追加 5. readTargetBPM() : 目標BPM読み取り関数
// ==========================================
float readTargetBPM() {
  int rawValue = analogRead(POT_PIN); // 0 〜 1023 を取得
  
  // 0〜1023のアナログ値を、BPM 100 〜 160 の範囲に変換する
  // ※可変抵抗の向きが逆(回すと下がる)場合は、map(rawValue, 0, 1023, 160, 100); にしてください
  long mappedBPM = map(rawValue, 0, 1023, 100, 160);
  
  return (float)mappedBPM;
}