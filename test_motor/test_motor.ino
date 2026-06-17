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
int beatcount  = 40;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0; 
// ★ゲイン調整用パラメータ
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
  
  updatePWM(80); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. シリアルプロッタの送信欄から目標BPM変更を受け付ける
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') targetBPM = 100.0;
    if (cmd == '2') targetBPM = 120.0;
    if (cmd == '3') targetBPM = 140.0;
    // ※グラフ乱れ防止のため、変更時のテキスト出力は削除しています
  }

  // 3. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; 
    
    // 【シリアルプロッタ用の表示形式】
    // 形式: Target:120.0,Current:115.0,PWM:135
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
      // プロッタのスケールが崩れるのを防ぐため、文字出力をコメントアウト
      // Serial.println("★Beat!"); 
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
  // もしPIDが「パワーを0以下にしろ(減速しろ)」と指示してきたらショートブレーキ！
  if (pwmValue <= 0) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, HIGH);
    currentPWM = 0;
  } 
  // 通常の加速・等速回転時
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
  
  // 【修正】I項（積分）がしっかりパワーを出せるように制限枠を大きく広げる
  integral = constrain(integral, -2000, 2000);

  float derivative = error - previous_error;

  // 【修正】80だと速すぎるため、120BPMに近くなるような小さめの基準値に変更
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