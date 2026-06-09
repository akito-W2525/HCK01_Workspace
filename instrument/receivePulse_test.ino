// ==========================================
// 1. receivePulse() : アナログ読み取りとエッジ検出
// ==========================================
void receivePulse() {
  // アナログ値を読み取る (50 〜 600)
  int sensorValue = analogRead(SENSOR_PIN);

  // 現在の状態がしきい値(300)を超えているか判定
  bool currentSensorState = (sensorValue > THRESHOLD);

  // 【エッジ検出】
  // 前回が false(遮断) で，今回が true(スリット) に「変化した瞬間」を見る
  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = millis();

    // チャタリング防止のための10ms待機(要変更)
    if (currentTime - lastPulseTime > 10) {
      pulseDetected = true; // 拍として認定
      lastPulseTime = currentTime;
    }
  }

  // 次回の比較のために、現在の状態を保存しておく
  lastSensorState = currentSensorState;
}
