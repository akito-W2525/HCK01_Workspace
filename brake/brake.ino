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
unsigned long currentDuration = 0;  // 計算された現在のスリット間隔(マイクロ秒)

// --- ★ブレーキテスト用追加変数 ---
bool isMotorRunning = false;      // モーターが回転中かどうか
bool isBraking = false;           // ブレーキテスト中かどうか
unsigned long brakeStartTime = 0; // ブレーキをかけた時刻(ミリ秒)
int extraSlitCount = 0;           // ブレーキ後の余分な拍（スリット）の数

// --- PIDとPWM用変数 ---
int currentPWM = 0;
float targetBPM = 120.0;       
float Kp = 1.0, Ki = 0.1, Kd = 0.05;
float integral = 0, previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.println("=====================================");
  Serial.println("  Reverse Brake Test Ready.");
  Serial.println("  シリアルモニタから以下の文字を送信してください：");
  Serial.println("  [g] : モーター回転スタート");
  Serial.println("  [s] : 逆相ブレーキ作動 (Stop)");
  Serial.println("=====================================");
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. シリアルモニタからの操作を受付
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    // 'g' が送信されたらスタート
    if (cmd == 'g' || cmd == 'G') {
      Serial.println("\n>>> 回転スタート <<<");
      slitCount = 0;
      extraSlitCount = 0;
      isMotorRunning = true;
      isBraking = false;
      updatePWM(200); // 200のスピードで回す（適宜調整してください）
    } 
    // 's' が送信されたらブレーキテスト開始
    else if (cmd == 's' || cmd == 'S') {
      if (isMotorRunning) {
        Serial.println("\n>>> 逆相ブレーキ作動！ <<<");
        ReverseBrake(); 
        isMotorRunning = false;
        isBraking = true;
        brakeStartTime = millis(); // ブレーキ開始時刻を記録
      }
    }
  }

  // 3. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; // フラグをリセット
    
    if (isMotorRunning) {
      // 通常回転中の検知（回転中の様子を確認用）
      Serial.print("S:");
      Serial.print(slitCount);
      Serial.print(" D(us):");
      Serial.print(currentDuration);
      Serial.print(" BPM:");
      Serial.println(currentBPM);
    } 
    else if (isBraking) {
      // ブレーキ作動中の検知（＝余分な拍）
      extraSlitCount++;
      Serial.print("【警告】余分なスリットを検知: 計 ");
      Serial.print(extraSlitCount);
      Serial.println(" 回");
    }
  }

  // 4. ブレーキ作動から2秒（2000ms）経過したら結果を発表
  if (isBraking && (millis() - brakeStartTime > 2000)) {
    isBraking = false; // テスト終了
    
    Serial.println("\n--- ブレーキテスト結果 ---");
    if (extraSlitCount == 0) {
      Serial.println("【合格】余分な拍の検知は 0 回でした！完璧に停止しています．");
    } else {
      Serial.print("【不合格】余分な拍が ");
      Serial.print(extraSlitCount);
      Serial.println(" 回発生しました．");
      Serial.println(" -> ReverseBrake() 内の delay(50) の数値を調整してください．");
    }
    Serial.println("=====================================");
    Serial.println("再度テストする場合は [g] を送信してください．");
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
    
    // チャタリング防止の1ms（1,000us）
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;

      unsigned long duration = currentTime - previousSlitTime;
      previousSlitTime = currentTime;

      // 上限の2000msも、マイクロ秒（2,000,000us）に直す
      if (duration > 0 && duration < 2000000) {
        // ループで表示するためにグローバル変数に保存
        currentDuration = duration; 
        
        // 計算用定数を1000倍する (1875.0 * 1000 = 1875000.0)
        currentBPM = 1875000.0 / (float)duration;
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
  delay(100); // ★ここの数値を変更して調整します
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, HIGH);
  currentPWM = 0;
}