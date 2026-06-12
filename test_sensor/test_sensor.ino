// --- ピン定義 ---
const int SENSOR_PIN = A0; 
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
int beatcount  = 37;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; // ★初期の目標BPM
// ★チューニング用のパラメータ（まずはPだけ入れてテストします）
float Kp = 1.0;  // 比例（現在のズレに対するパワー）
float Ki = 0.1;  // 積分（過去のズレの蓄積に対するパワー）
float Kd = 0.05; // 微分（急激な変化に対するブレーキ）
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.println("=====================================");
  Serial.println("  PID Control Test Ready.");
  Serial.println("  シリアルモニタから以下の数字を送信して目標BPMを変更できる：");
  Serial.println("  [1] : Target BPM 100");
  Serial.println("  [2] : Target BPM 120");
  Serial.println("  [3] : Target BPM 140");
  Serial.println("=====================================");
  
  // モーターを初期スピードで回し始めます
  updatePWM(135); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. シリアルモニタからの目標BPM変更を受け付ける
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') targetBPM = 100.0;
    if (cmd == '2') targetBPM = 120.0;
    if (cmd == '3') targetBPM = 140.0;
    
    if (cmd == '1' || cmd == '2' || cmd == '3') {
      Serial.print("\n>>> 目標BPMを [");
      Serial.print(targetBPM);
      Serial.println("] に変更しました <<<\n");
    }
  }

  // 3. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; // フラグをリセット
    
    // 【PIDの働きを可視化する表示】
    Serial.print("Target:");
    Serial.print(targetBPM);
    Serial.print(" | Current:");
    Serial.print(currentBPM);
    Serial.print(" | PWM:");
    Serial.println(currentPWM); // PIDが計算した結果のパワー

    if (slitCount >= beatcount) {
      slitCount = 0; 
      Serial.println("★Beat!"); 
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
        currentBPM = ( 60000000.0 / ((float)duration * beatcount*2));
        slitCount++;

        // ★★★ここで毎スリットPIDを呼び出し、モーター速度を自動調整する★★★
        updatePID(targetBPM, currentBPM);
      }
    }
  }
  
  lastSensorState = currentSensorState;
}

// ==========================================
// 2. updatePWM() : モーター出力制御関数
// ==========================================
void updatePWM(int pwmValue) {
  pwmValue = constrain(pwmValue, 0, 255);
  analogWrite(MOTOR_IN1, pwmValue);
  digitalWrite(MOTOR_IN2, LOW);
  currentPWM = pwmValue;
}

// ==========================================
// 3. updatePID() : 回転速度安定化関数
// ==========================================
int updatePID(float targetBPM, float currentBPM) {
  float error = targetBPM - currentBPM;
  integral += error;
  
  // 積分(I)の暴走（ワインドアップ）を防ぐための安全制限を追加
  integral = constrain(integral, -200, 200);

  float derivative = error - previous_error;

  float output = (Kp * error) + (Ki * integral) + (Kd * derivative);
  previous_error = error;

  int newPWM = currentPWM + (int)output;
  newPWM = constrain(newPWM, 0, 255);

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