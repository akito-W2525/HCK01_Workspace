// --- ピン定義 ---
const int SENSOR_PIN = A0; // フォトインタラプタのアナログピン
const int MOTOR_IN1 = 9;   // モータードライバ IN1
const int MOTOR_IN2 = 10;  // モータードライバ IN2

// --- しきい値と状態管理 ---
const int THRESHOLD = 300;     // 50と600の中間あたり
bool lastSensorState = false;  // 前回のセンサ状態
unsigned long lastPulseTime = 0; // チャタリング防止用
bool pulseDetected = false;    // 拍検知フラグ

// --- 拍カウントとBPM計算用変数 ---
int slitCount = 0;                  // スリットの通過回数を数える
unsigned long previousSlitTime = 0; // スリット間隔（duration）計測用
float currentBPM = 0.0;             // 計算された現在のBPM
unsigned long currentDuration = 0;  // ★追加：計算された現在のスリット間隔(マイクロ秒)
int beatcount  = 37;

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0;       
float Kp = 1.0, Ki = 0.1, Kd = 0.05;
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.println("Motor & Sensor Integration Test Ready.");
  
  // モーターを定速で回し始めます（200は適宜調整してください）
  updatePWM(135); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; // フラグをリセット
    
    // 【修正】スリット間隔(マイクロ秒)の表示を追加
    Serial.print("S:");
    Serial.print(slitCount);
    Serial.print(" D(us):");
    Serial.print(currentDuration);
    Serial.print(" BPM:");
    Serial.println(currentBPM);

    // 32回読み取ったら「1拍完了」の特別な表示を出す
    if (slitCount >= beatcount) {
      slitCount = 0; // カウンターをリセットして次の拍へ
      
      // 処理落ちを防ぐため、短い文字に変更
      Serial.println("★Beat!"); 
    }
  }
}

// ==========================================
// 1. receivePulse() : アナログ読み取りとエッジ検出 (超高精度版)
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = (sensorValue > THRESHOLD);

  if (!lastSensorState && currentSensorState) {
    // micros() を使う（マイクロ秒単位で取得）
    unsigned long currentTime = micros(); 
    
    // チャタリング防止の10msを、マイクロ秒（10,000us）に直す
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;

      unsigned long duration = currentTime - previousSlitTime;
      previousSlitTime = currentTime;

      // 上限の2000msも、マイクロ秒（2,000,000us）に直す
      if (duration > 0 && duration < 2000000) {
        // ★ループで表示するためにグローバル変数に保存
        currentDuration = duration; 
        
        // 計算用定数を1000倍する (1875.0 * 1000 = 1875000.0)
        currentBPM = ( 60000000.0 / ((float)duration * beatcount*2));

        slitCount++;
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