// ==========================================
// setup内の英語化
// ==========================================
void setup() {
  Serial.begin(115200); 
  Serial.println("=========================================");
  Serial.println("Instrument Test Mode Started.");
  Serial.println("Waiting for slits...");
  Serial.println("=========================================");
}

// （loop関数などはそのまま）

// ==========================================
// シリアルモニタ確認用 出力関数（英語化）
// ==========================================
void printNote(Note note) {
  unsigned long duration_ms = beatInterval * note.length;

  Serial.print("▶ PLAY | ");
  
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