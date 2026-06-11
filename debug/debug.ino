// --- ピン定義 ---
const int SENSOR_PIN = A0; 
const int MOTOR_IN1 = 9;
const int MOTOR_IN2 = 10;

// --- しきい値と状態管理 ---
const int THRESHOLD = 300;     
bool lastSensorState = false;  
unsigned long lastPulseTime = 0;
bool pulseDetected = false;    

// --- デバッグ用変数 ---
unsigned long lastDebugPrintTime = 0;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0;       
float currentBPM = 120.0;      
float Kp = 1.0, Ki = 0.1, Kd = 0.05;
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.println("=== Debug Test Started ===");
  
  // モーターを定速で回す指示（150は適宜変更してください）
  updatePWM(200); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. 拍が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; // フラグをリセット
    unsigned long currentTime = millis();
    
    Serial.print("★ Pulse Detected! Time: ");
    Serial.println(currentTime);
  }

  // 3. 【デバッグ用】0.5秒に1回、現在のセンサのアナログ値を表示する
  unsigned long currentTime = millis();
  if (currentTime - lastDebugPrintTime > 500) {
    int rawValue = analogRead(SENSOR_PIN);
    Serial.print("[Debug] Current Sensor Value: ");
    Serial.println(rawValue);
    lastDebugPrintTime = currentTime;
  }
}

// ==========================================
// 1. receivePulse() : アナログ読み取りとエッジ検出
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = (sensorValue > THRESHOLD);

  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = millis();
    if (currentTime - lastPulseTime > 10) {
      pulseDetected = true; 
      lastPulseTime = currentTime;
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