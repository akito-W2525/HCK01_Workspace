// --- ピン定義 ---
const int SENSOR_PIN = A0; // 楽器側のフォトインタラプタ用アナログピン

// --- しきい値と状態管理（超安定ヒステリシス版） ---
const int THRESHOLD_HIGH = 400; // ここを超えたらスリット
const int THRESHOLD_LOW = 200;  // ここを下回ったら壁
bool currentSensorState = false;
bool lastSensorState = false;  
unsigned long lastPulseTime = 0; 
bool pulseDetected = false;    

// --- 拍カウントとBPM計算用変数 ---
int slitCount = 0;                  
unsigned long previousSlitTime = 0; 
float currentBPM = 0.0;             
int beatcount  = 40; // ★指揮者側と同じ値（40）に合わせてください

void setup() {
  Serial.begin(115200);
  
  // プロッタのノイズになるため、初期のテキストは最小限にします
  Serial.println("Instrument_Current_BPM"); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; 
    
    // 【シリアルプロッタ用の表示形式】
    Serial.print("Current:");
    Serial.println(currentBPM);

    if (slitCount >= beatcount) {
      slitCount = 0; 
    }
  }
}

// ==========================================
// 1. receivePulse() : ノイズに強いヒステリシス版
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);

  // 400以上でON、200以下でOFF
  if (sensorValue > THRESHOLD_HIGH) {
    currentSensorState = true;
  } else if (sensorValue < THRESHOLD_LOW) {
    currentSensorState = false;
  }

  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = micros(); 
    
    // チャタリング防止 (1000us)
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;

      unsigned long duration = currentTime - previousSlitTime;
      previousSlitTime = currentTime;

      // 異常値弾き
      if (duration > 0 && duration < 2000000) {
        
        // ★指揮者側と全く同じBPM計算式 (8分音符基準)
        currentBPM = ( 60000000.0 / ((float)duration * beatcount * 2));
        slitCount++;
      }
    }
  }
  
  lastSensorState = currentSensorState;
}