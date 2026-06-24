// ==========================================
// 楽器側メインシステム (シリアルモニタ確認用テスト版)
// ==========================================

// --- ピン定義 ---
const int SENSOR_PIN = A0; 

// --- しきい値とノイズフィルタ設定 ---
const int THRESHOLD = 300;     
bool lastSensorState = false;  
unsigned long lastPulseTime = 0; 
unsigned long previousSlitTime = 0; 

// --- 拍カウント用変数 ---
bool pulseDetected = false;    
unsigned long beatInterval = 0; // スリット1つあたりの時間（ミリ秒）

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
  Serial.println("=========================================");
  Serial.println("Instrument Test Mode Started.");
  Serial.println("Waiting for slits...");
  Serial.println("=========================================");
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
      
      // ★機械語ではなく、人間が読める形式でシリアルモニタに出力
      printNote(n);
      
      beatCounter = n.length;
      currentNoteIndex++;
      
      if (currentNoteIndex >= melodyLength) {
        currentNoteIndex = 0;
        Serial.println("--- 楽譜ループ ---");
      }
    }
    beatCounter--;
  }
}

// ==========================================
// シリアルモニタ確認用 出力関数（英語化）
// ==========================================
void printNote(Note note) {
  unsigned long duration_ms = beatInterval * note.length;

  Serial.print("> PLAY | "); 
  
  if (note.velocity == 0) {
    Serial.print("REST (Pitch:");
    Serial.print(note.pitch);
    Serial.print(")  ");
  } else {
    Serial.print("Pitch: ");
    Serial.print(note.pitch);
    Serial.print("    ");
  }

  Serial.print("| Length: ");
  Serial.print(note.length);
  Serial.print(" | Actual_Time: ");
  Serial.print(duration_ms);
  Serial.print(" ms | Velocity: ");
  Serial.println(note.velocity);
}

// ==========================================
// センサー読み取り関数内（文字出力部分の英語化）
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
        Serial.println("[System] First slit detected."); // 英語化
      } 
      else {
        unsigned long duration = currentTime - previousSlitTime;
        previousSlitTime = currentTime;

        if (duration > 0 && duration < 2000000) {
          beatInterval = duration / 1000;
          pulseDetected = true; 
          
          Serial.print("[Sensor] Slit -> Interval: "); // 英語化
          Serial.print(beatInterval);
          Serial.println(" ms");
        } 
        else {
          previousSlitTime = currentTime;
          Serial.println("[System] Long gap detected. Resetting timing."); // 英語化
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}