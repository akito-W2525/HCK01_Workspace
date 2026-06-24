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

// --- 拍カウント・BPM用変数 ---
bool pulseDetected = false;    
unsigned long beatInterval = 0; // 1拍あたりの時間（ミリ秒）
int slitCount = 0;              // ★追加: スリットのカウント
const int beatcount = 40;       // ★追加: 40回で1音(1拍)とする
float currentBPM = 0.0;         // ★追加: 現在のBPM

// --- 楽譜・ノート定義 ---
struct Note {
  byte pitch;
  byte length;
  byte velocity;
};

// 楽器の楽譜（1 = スリット40回分 / 8分音符相当）
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
  Serial.println("Waiting for slits (40 slits = 1 beat)...");
  Serial.println("=========================================");
}

void loop() {
  // 1. 常にセンサの値を読み取る
  receivePulse();

  // 2. ★40スリット検知（= 1拍進行）されたら楽譜を進める
  if (pulseDetected) {
    pulseDetected = false;

    if (beatCounter <= 0) {
      Note n = Melody[currentNoteIndex];
      
      // 視覚的でシンプルな出力
      printNote(n);
      
      beatCounter = n.length;
      currentNoteIndex++;
      
      if (currentNoteIndex >= melodyLength) {
        currentNoteIndex = 0;
      }
    }
    beatCounter--;
  }
}

// ==========================================
// シリアルモニタ確認用 出力関数
// ==========================================
void printNote(Note note) {
  // 余計な情報を削り、タイミングとBPMを視覚的に強調
  Serial.print("【 ★ BEAT 】 BPM: ");
  Serial.print(currentBPM);
  Serial.print("  |  ");
  
  if (note.velocity == 0) {
    Serial.println("REST (休符)");
  } else {
    Serial.print("PLAY -> Pitch: ");
    Serial.println(note.pitch);
  }
}

// ==========================================
// センサー読み取り関数（究極ノイズ除去版）
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
          
          // 指揮者側と同じ計算式でBPMを割り出す
          currentBPM = (60000000.0 / ((float)duration * beatcount * 2));
          slitCount++;

          // ★ 40回スリットを検知したら「1音（1拍）」とする！
          if (slitCount >= beatcount) {
            beatInterval = (duration * beatcount) / 1000; // 1拍のミリ秒
            pulseDetected = true; 
            slitCount = 0; // カウントリセット
          }

        } 
        else {
          // 指揮者側のキュー出し待ち等で長時間空いた場合はリセット
          previousSlitTime = currentTime;
          slitCount = 0; 
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}