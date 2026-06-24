// ==========================================
// 楽器側メインシステム (本番・輪唱対応版)
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
unsigned long beatInterval = 0; // 1拍（40スリット分）の時間（ミリ秒）
int slitCount = 0;              // スリットのカウント
const int beatcount = 40;       // 40回で1音(1拍)とする

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
  // ※楽器側はセンサーの読み取りとシリアル送信のみを行うため、ピン設定は最小限でOKです
}

void loop() {
  // 1. 常にセンサの値を読み取り、ノイズを除去してスリットを検知
  receivePulse();

  // 2. ★40スリット検知（= 1拍進行）されたら楽譜を進める
  if (pulseDetected) {
    pulseDetected = false;

    // 楽譜の進行とシリアル送信処理
    if (beatCounter <= 0) {
      Note n = Melody[currentNoteIndex];
      sendNote(n);
      
      // 次の音へ進むためのカウントを設定
      beatCounter = n.length;
      currentNoteIndex++;
      
      // 楽譜の最後まで来たら最初に戻る
      if (currentNoteIndex >= melodyLength) {
        currentNoteIndex = 0;
      }
    }
    beatCounter--;
  }
}

// ==========================================
// UDP/Processing送信関数
// ==========================================
void sendNote(Note note) {
  // 楽譜の音長と拍の間隔をかけて ms に変換
  int duration_ms = beatInterval * note.length;

  // 効率化：4バイト分のデータを入れる配列を作って一気に送信
  byte buf[4];
  buf[0] = note.pitch;               // 1バイト目: 音の高さ
  buf[1] = highByte(duration_ms);    // 2バイト目: 音長の上の桁
  buf[2] = lowByte(duration_ms);     // 3バイト目: 音長の下の桁
  buf[3] = note.velocity;            // 4バイト目: 音の強さ

  Serial.write(buf, 4);
}

// ==========================================
// センサー読み取り関数（40スリットで1拍カウント）
// ==========================================
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = false; 

  // ソフトウェア・ダブルチェック（50us待機して火花ノイズを除去）
  if (sensorValue > THRESHOLD) {
    delayMicroseconds(50); 
    int confirmValue = analogRead(SENSOR_PIN);
    if (confirmValue > THRESHOLD) {
      currentSensorState = true; 
    }
  }

  // 立ち上がりエッジの検出
  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = micros(); 
    
    // チャタリング防止時間（1000us）
    if (currentTime - lastPulseTime > 1000) {
      lastPulseTime = currentTime;

      // 初回の検知は時間を記録するだけ
      if (previousSlitTime == 0) {
        previousSlitTime = currentTime;
      } 
      else {
        unsigned long duration = currentTime - previousSlitTime;
        previousSlitTime = currentTime;

        // durationが2秒未満なら正常なスリット間隔として処理
        if (duration > 0 && duration < 2000000) {
          slitCount++; // スリット通過回数をカウント

          // ★ 40回スリットを検知したら「1音（1拍）」としてProcessingへ知らせる
          if (slitCount >= beatcount) {
            // 1拍の長さをミリ秒に変換 (duration(us) * 40回 / 1000)
            beatInterval = (duration * beatcount) / 1000;
            
            pulseDetected = true; 
            slitCount = 0; // カウントリセット
          }
        } 
        else {
          // 指揮者側のキュー出し待ち等で長時間空いた場合はリセット
          previousSlitTime = currentTime;
          slitCount = 0; // ★カウントもリセットして次の指示に備える
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}