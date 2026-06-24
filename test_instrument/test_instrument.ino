// ==========================================
// 楽器側メインシステム (シリアルモニタ視覚的テスト版)
// ==========================================

// --- ピン定義 ---
const int SENSOR_PIN = A0; 

// --- しきい値とノイズフィルタ設定 ---
const int THRESHOLD = 300;     
bool lastSensorState = false;  
unsigned long lastPulseTime = 0; 
unsigned long previousSlitTime = 0; 

// --- 拍カウントとBPM計算用変数 ---
bool pulseDetected = false;    
unsigned long beatInterval = 0; 
float currentBPM = 0.0;     // ★追加：BPM計算用
int beatcount = 40;         // ★追加：指揮者側と同じスリット数

// --- 楽譜・ノート定義 ---
struct Note {
  byte pitch;
  byte length;
  byte velocity;
};

// 楽器の楽譜（1 = スリット1つ分 / 8分音符相当）
Note Melody[] = {
  {60, 2, 200}, {62, 2, 200}, {64, 2, 200}, {65, 2, 200},
  {64, 2, 200}, {62, 2, 200}, {60, 2, 200}, {128,  2, 0},
  {64, 2, 200}, {65, 2, 200}, {67, 2, 200}, {69, 2, 200},
  {67, 2, 200}, {65, 2, 200}, {64, 2, 200}, {128,  2, 0},
  {60, 2, 200}, {128, 2, 0},  {60, 2, 200}, {128, 2, 0},
  {60, 2, 200}, {128, 2, 0},  {60, 2, 200}, {128,  2, 0},
  {60, 1, 200}, {60, 1, 200}, {62, 1, 200}, {62, 1, 200}, 
  {64, 1, 200}, {64, 1, 200}, {65, 1, 200}, {65, 1, 200},
  {64, 2, 200}, {62, 2, 200}, {60, 2, 200}, {128,  2, 0}
};

const int melodyLength = sizeof(Melody) / sizeof(Melody[0]);
int currentNoteIndex = 0;
int beatCounter = 0;

void setup() {
  Serial.begin(115200); 
  Serial.println("Ready.");
}

void loop() {
  // 1. 常にセンサの値を読み取り、ノイズを除去してスリットを検知
  receivePulse();

  // 2. スリットが検知（= 1拍進行）されたら楽譜を進める
  if (pulseDetected) {
    pulseDetected = false;

    // 楽譜の進行とシリアル送信処理
    if (beatCounter <= 0) {
      Note n = Melody[currentNoteIndex];
      
      // 音が切り替わるタイミングで表示
      printNote(n);
      
      beatCounter = n.length;
      currentNoteIndex++;
      
      if (currentNoteIndex >= melodyLength) {
        currentNoteIndex = 0;
        Serial.println("--- LOOP ---");
      }
    } else {
      // ★音が伸びている最中（スリット通過）を視覚的なバーで表現
      Serial.println("  |");
    }
    beatCounter--;
  }
}

// ==========================================
// シリアルモニタ確認用 出力関数
// ==========================================
void printNote(Note note) {
  if (note.velocity == 0) {
    Serial.print("[REST]   "); // 休符
  } else {
    Serial.print("[NOTE:");    // 発音
    Serial.print(note.pitch);
    Serial.print("] ");
  }
  
  // BPMの表示
  Serial.print(" BPM:");
  Serial.println(currentBPM);
}

// ==========================================
// センサー読み取り関数内
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = false; 

  if (sensorValue > THRESHOLD) {
    delayMicroseconds(50); 
    int confirmValue = analogRead(SENSOR_PIN);
    if (confirmValue > THRESHOLD) {
      currentSensorState = true; 
    }
  }

  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = micros(); 
    
    if (currentTime - lastPulseTime > 1000) {
      lastPulseTime = currentTime;

      if (previousSlitTime == 0) {
        previousSlitTime = currentTime;
      } 
      else {
        unsigned long duration = currentTime - previousSlitTime;
        previousSlitTime = currentTime;

        if (duration > 0 && duration < 2000000) {
          beatInterval = duration / 1000;
          
          // ★指揮者側と同じ計算式でBPMを算出
          currentBPM = (60000000.0 / ((float)duration * beatcount * 2));
          
          pulseDetected = true; 
        } 
        else {
          previousSlitTime = currentTime;
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}