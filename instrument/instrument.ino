// ==========================================
// 楽器側メインシステム (本番・1ループ終了・LEDマトリクス対応版)
// ==========================================
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

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
const int beatcount = 45;       // 45回で1音(1拍)とする

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
bool isFinished = false; // 1ループ演奏が終わったかどうかのフラグ

// ==========================================
// LEDマトリクス用データと変数
// ==========================================
uint8_t tadpole1[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},{0,1,1,1,1,0,0,0,0,0,0,0},
  {1,0,0,0,0,1,1,0,0,0,0,0},{1,0,1,0,0,0,0,1,1,1,1,1},
  {1,0,0,0,0,0,0,1,1,0,0,0},{1,0,0,0,0,0,1,1,0,0,0,0},
  {0,1,1,1,1,1,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};
uint8_t tadpole2[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},{0,1,1,1,1,0,0,0,0,0,0,0},
  {1,0,0,0,0,1,1,0,0,0,0,0},{1,0,1,0,0,0,0,1,1,0,0,0},
  {1,0,0,0,0,0,0,1,1,1,1,1},{1,0,0,0,0,0,1,1,0,0,0,0},
  {0,1,1,1,1,1,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};
uint8_t frog1[8][12] = { // 待機（口閉じ）
  {0,0,1,1,0,0,0,0,1,1,0,0},{0,1,0,0,1,0,0,1,0,0,1,0},
  {0,1,0,0,1,1,1,1,0,0,1,0},{1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,1,1,1,1,1,0,0,1},{1,0,0,1,0,0,0,0,1,0,0,1},
  {1,0,0,1,0,0,0,0,1,0,0,1},{0,1,0,0,1,1,1,1,0,0,1,0}
};
uint8_t frog2[8][12] = { // 発音（口開き）
  {0,0,1,1,0,0,0,0,1,1,0,0},{0,1,0,0,1,0,0,1,0,0,1,0},
  {0,1,0,0,1,1,1,1,0,0,1,0},{1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},{1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,1,1,1,1,1,0,0,1},{0,1,0,0,0,0,0,0,0,0,1,0}
};

// 状態管理
enum SystemState { STATE_STANDBY, STATE_PLAYING };
SystemState currentState = STATE_STANDBY;

unsigned long lastTadpoleUpdate = 0;
bool tadpoleToggle = true;

bool isMouthOpen = false;
unsigned long mouthOpenTime = 0;
const unsigned long MOUTH_OPEN_DURATION = 150; // カエルが口を開ける時間(ms)

// ==========================================
// setup & loop
// ==========================================
void setup() {
  Serial.begin(115200); 
  matrix.begin();
  // 初期状態はオタマジャクシ
  matrix.renderBitmap(tadpole1, 8, 12); 
  lastTadpoleUpdate = millis();
}

void loop() {
  // 1. 常にセンサの値を読み取り、ノイズを除去してスリットを検知
  receivePulse();

  // 2. 40スリット検知（= 1拍進行）されたら楽譜を進める
  if (pulseDetected) {
    pulseDetected = false;

    if (!isFinished) {
      // 楽譜の進行とシリアル送信処理
      if (beatCounter <= 0) {
        Note n = Melody[currentNoteIndex];
        sendNote(n);
        
        // ★ 音が鳴る（休符でない）ならカエルの口を開ける
        if (n.velocity > 0) {
          isMouthOpen = true;
          mouthOpenTime = millis();
          matrix.renderBitmap(frog2, 8, 12);
        }
        
        // 次の音へ進むためのカウントを設定
        beatCounter = n.length;
        currentNoteIndex++;
        
        // 楽譜の最後まで来たら演奏終了フラグを立てる
        if (currentNoteIndex >= melodyLength) {
          isFinished = true;
          setSystemState(STATE_STANDBY); // 終了したらオタマジャクシに戻る
        }
      }
      beatCounter--;
    }
  }

  // 3. LEDアニメーションのノンブロッキング更新
  updateLEDAnimation();
}

// ==========================================
// LEDアニメーション制御関数
// ==========================================
void setSystemState(SystemState newState) {
  if (currentState != newState) {
    currentState = newState;
    if (currentState == STATE_STANDBY) {
      // 待機に戻ったらオタマジャクシを描画
      matrix.renderBitmap(tadpole1, 8, 12);
      lastTadpoleUpdate = millis();
      tadpoleToggle = true;
    } else {
      // 演奏が始まったらカエル（口閉じ）に進化
      matrix.renderBitmap(frog1, 8, 12);
      isMouthOpen = false;
    }
  }
}

void updateLEDAnimation() {
  unsigned long now = millis();
  
  if (currentState == STATE_STANDBY) {
    // 待機中：500msごとにオタマジャクシを泳がせる
    if (now - lastTadpoleUpdate >= 500) {
      lastTadpoleUpdate = now;
      tadpoleToggle = !tadpoleToggle;
      if (tadpoleToggle) {
        matrix.renderBitmap(tadpole1, 8, 12);
      } else {
        matrix.renderBitmap(tadpole2, 8, 12);
      }
    }
  } 
  else if (currentState == STATE_PLAYING) {
    // 演奏中：発音時に口を開け、指定時間経過したら閉じる
    if (isMouthOpen) {
      if (now - mouthOpenTime >= MOUTH_OPEN_DURATION) {
        isMouthOpen = false;
        matrix.renderBitmap(frog1, 8, 12);
      }
    }
  }
}

// ==========================================
// UDP/Processing送信関数
// ==========================================
void sendNote(Note note) {
  int duration_ms = beatInterval * note.length;
  byte buf[4];
  buf[0] = note.pitch;               
  buf[1] = highByte(duration_ms);    
  buf[2] = lowByte(duration_ms);     
  buf[3] = note.velocity;            
  Serial.write(buf, 4);
}

// ==========================================
// センサー読み取り関数（45スリットで1拍カウント）
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
          slitCount++; 

          // スリットを正常に読み取っている ＝ 演奏中状態にする
          if (currentState == STATE_STANDBY && !isFinished) {
            setSystemState(STATE_PLAYING);
          }

          if (slitCount >= beatcount) {
            beatInterval = (duration * beatcount) / 1000;
            pulseDetected = true; 
            slitCount = 0; 
          }
        } 
        else {
          // 指揮者側のストップ等で長時間空いた場合はリセット
          previousSlitTime = currentTime;
          slitCount = 0; 
          
          currentNoteIndex = 0;
          beatCounter = 0;
          isFinished = false;
          
          // 長時間止まったら待機状態（オタマジャクシ）へ
          setSystemState(STATE_STANDBY);
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}