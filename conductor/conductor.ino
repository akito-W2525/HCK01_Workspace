// --- ピン定義 ---
const int SENSOR_PIN = A0; // フォトインタラプタ (RPI-574)
const int POT_PIN = A1;    // 可変抵抗（ポテンショメータ）
const int MOTOR_IN1 = 9;   // TA7291P IN1 (PWM)
const int MOTOR_IN2 = 10;  // TA7291P IN2 (PWM)

// --- センサしきい値と状態管理 ---
const int THRESHOLD = 300;
bool lastSensorState = false;
unsigned long lastSlitTime = 0;   // 前回の「スリット検知」の時刻
unsigned long lastPulseTime = 0;  // チャタリング防止用

// --- 拍カウント管理 ---
int slitCount = 0;          // スリットの通過回数を数えるカウンター
bool beatDetected = false;  // 1拍（スリット8回）完了フラグ

// --- モーター・PID制御用変数 ---
int currentPWM = 0;
float Kp = 2.0;  // 実機に合わせて調整してください
float Ki = 0.5;
float Kd = 0.1;
float integral = 0;
float previous_error = 0;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  Serial.println("Conductor System Initialized.");
  
  // 最初は少し回してスタートさせる場合
  updatePWM(100);
}

void loop() {
  // 1. 常にセンサの監視を行う
  receivePulse();

  // 2. 1拍（スリット8回分）が完了したときの処理
  if (beatDetected) {
    beatDetected = false; // フラグをリセット
    Serial.println("★ 1 Beat Completed (8 Slits) ★");
    
    // ここで他の楽器用Arduinoに同期用の信号を送るなどの処理（詳細設計2.4など）に繋げます
  }
}