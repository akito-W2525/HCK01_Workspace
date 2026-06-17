// --- ピン定義 ---
const int SENSOR_PIN = A0; // 楽器側のフォトインタラプタのアナログピン

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
int beatcount  = 40; // 指揮者側と必ず合わせる

void setup() {
  Serial.begin(115200);

  // プロッタのノイズになるため、初期のテキストは最小限にします
  Serial.println("Instrument_BPM"); 
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. 拍（スリット）が検知されたら実行される処理
  if (pulseDetected) {
    pulseDetected = false; 
    
    // 【シリアルプロッタ用の表示形式】
    // 楽器側が認識している現在のBPMだけを出力します
    Serial.print("Instrument_BPM:");
    Serial.println(currentBPM);

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
    
    // チャタリング防止（1000us）
    if (currentTime - lastPulseTime > 1000) {
      pulseDetected = true; 
      lastPulseTime = currentTime;

      unsigned long duration = currentTime - previousSlitTime;
      previousSlitTime = currentTime;

      if (duration > 0 && duration < 2000000) {
        currentDuration = duration; 
        
        // BPMの計算 (8分音符基準) - 指揮者側と全く同じ計算式
        currentBPM = ( 60000000.0 / ((float)duration * beatcount * 2));
        slitCount++;
      }
    }
  }
  
  lastSensorState = currentSensorState;
}