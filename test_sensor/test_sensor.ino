// --- ピン定義 ---
const int SENSOR_PIN = A0; // ★アナログピン要変更
const int MOTOR_IN1 = 9;
const int MOTOR_IN2 = 10;

// --- しきい値と状態管理 ---
const int THRESHOLD = 300;     // 50と600の中間あたりに設定
bool lastSensorState = false;  // 前回のセンサ状態 (false=遮断, true=スリット)
unsigned long lastPulseTime = 0;
bool pulseDetected = false;    // 拍検知フラグ

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0;       // 目標BPM（可変抵抗で書き換わります）
float currentBPM = 120.0;      // 現在の実測BPM（実際はスリット間隔等から計算）
float Kp = 1.0, Ki = 0.1, Kd = 0.05;
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  // アナログピンは pinMode(SENSOR_PIN, INPUT) の記述を省略可能です

  Serial.println("Analog Sensor Test Ready.");
  
  // モーターを回すなら，以下のコメントアウトを外す
  // updatePWM(150); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. 拍が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; // フラグをリセット
    unsigned long currentTime = millis();
    
    Serial.print("Pulse Detected! Time: ");
    Serial.println(currentTime);
  }
}

// ==========================================
// 1. receivePulse() : アナログ読み取りとエッジ検出(楽器側)
// ==========================================
void receivePulse() {
  // アナログ値を読み取る (50 〜 600)
  int sensorValue = analogRead(SENSOR_PIN);
  
  // 現在の状態がしきい値(300)を超えているか判定
  bool currentSensorState = (sensorValue > THRESHOLD);

  // 【エッジ検出】
  // 前回が false(遮断) で，今回が true(スリット) に「変化した瞬間」を見る
  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = millis();
    
    // チャタリング防止のための10ms待機
    if (currentTime - lastPulseTime > 10) {
      pulseDetected = true; // 拍として認定
      lastPulseTime = currentTime;
    }
  }
  
  // 次回の比較のために、現在の状態を保存しておく
  lastSensorState = currentSensorState;
}

// ==========================================
// 2. updatePWM() : モーター出力制御関数
// ==========================================
void updatePWM(int pwmValue) {
  // 安全のためPWM値を0-255に制限
  pwmValue = constrain(pwmValue, 0, 255);
  
  // TA7291Pを用いた正転制御
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
  // 1. 逆相ブレーキ（一瞬だけ逆転）
  digitalWrite(MOTOR_IN1, LOW);
  analogWrite(MOTOR_IN2, 255);
  
  // 慣性を打ち消すための微小時間 (要調整)
  delay(50); 

  // 2. ショートブレーキ（モーターの両端子をHIGHにして固定）
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, HIGH);
  
  currentPWM = 0;
}

// ==========================================
// 5. readTargetBPM() : 目標BPM読み取り関数
// ==========================================
// 引数: なし
// 戻り値: float（目標BPM値）
// 処理: 可変抵抗のアナログ値を読み取り，仕様であるBPM 120〜180の範囲に変換して返す
float readTargetBPM() {
  int rawValue = analogRead(POT_PIN); // 0 〜 1023 の値を取得
  
  // map関数を使って、0〜1023 の範囲を 120〜180 の範囲に比例変換する
  long mappedBPM = map(rawValue, 0, 1023, 120, 180);
  
  return (float)mappedBPM;
}