// --- 楽器側のグローバル変数 ---
const int SENSOR_PIN = A0; // 楽器側のフォトインタラプタ
const int THRESHOLD = 300;
bool lastSensorState = false;
unsigned long lastPulseTime = 0;

// 楽器特有の変数
unsigned long previousSlitTime = 0; 
unsigned long currentInterval = 0; // スリットとスリットの間隔（ミリ秒）
bool noteTriggerFlag = false;      // 音を鳴らすタイミングを知らせるフラグ

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // 常にセンサを監視
  receivePulse();

  // フラグが立ったら（スリットを検知したら）
  if (noteTriggerFlag) {
    noteTriggerFlag = false; // フラグを下ろす
    
    // ここでsendNote() を呼び出してProcessingにデータを送る 
    // sendNote(currentInterval); のように、計算した間隔を渡してあげる
  }
}