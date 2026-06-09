// ==========================================
// 1. receivePulse() : 拍信号の受信関数（計画書準拠）
// ==========================================
// 処理: 円盤から入力される拍信号を受信し、内部タイミング情報を更新。
// ※アナログリードによるエッジ検出と、毎スリットのBPM計算・PID更新を行います。
void receivePulse() {
  int sensorValue = analogRead(SENSOR_PIN);
  bool currentSensorState = (sensorValue > THRESHOLD);

  // 立ち上がりエッジ（壁から穴へ変化した瞬間）を検知
  if (!lastSensorState && currentSensorState) {
    unsigned long currentTime = millis();
    
    // チャタリング防止（10ms以内は無視）
    if (currentTime - lastPulseTime > 10) {
      lastPulseTime = currentTime;
      
      // スリット間の時間（ミリ秒）を計算
      unsigned long duration = currentTime - lastSlitTime;
      lastSlitTime = currentTime;

      // 初回検知時などの異常な大きな値を排除
      if (duration > 0 && duration < 2000) {
        
        // 可変抵抗から目標BPMを取得（新設関数）
        float targetBPM = readTargetBPM();
        
        // 【重要】スリット1回分の時間から現在の瞬間BPMを算出 (7500.0 / duration)
        float currentBPM = 7500.0 / (float)duration;

        // 毎スリットごとにPID制御を実行してモーター速度を更新
        updatePID(targetBPM, currentBPM);
        
        // スリットの通過回数をインクリメント
        slitCount++;
        
        // デバッグ用表示（毎スリットの動きを確認できます）
        Serial.print("SlitCount: "); Serial.print(slitCount);
        Serial.print(" | Target: "); Serial.print(targetBPM);
        Serial.print(" | CurrentBPM: "); Serial.println(currentBPM);

        // 8回読み取ったら1拍完了
        if (slitCount >= 8) {
          slitCount = 0;       // カウンターをリセット
          beatDetected = true; // 1拍完了フラグを立てる
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}