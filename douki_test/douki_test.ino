// ==========================================
// 楽器側メインシステム (RST同期・同期精度確認版)
// ==========================================

const int SENSOR_PIN = A0; 
const int THRESHOLD = 300;     
bool lastSensorState = false;  
unsigned long lastPulseTime = 0; 
unsigned long previousSlitTime = 0; 

bool pulseDetected = false;    
unsigned long beatInterval = 0; 
float currentBPM = 0.0;
int beatcount = 40;
int slitCount = 0;

unsigned long startTime = 0;  // ★起動時刻記録用

struct Note {
  byte pitch;
  byte length;
  byte velocity;
};

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
  startTime = micros();  // ★起動時刻を記録
  Serial.print("Start t=");
  Serial.print(startTime);
  Serial.println("us");
  Serial.println("Ready.");
}

void loop() {
  receivePulse();

  if (pulseDetected) {
    pulseDetected = false;

    if (beatCounter <= 0) {
      Note n = Melody[currentNoteIndex];
      beatCounter = n.length;
      currentNoteIndex++;
      if (currentNoteIndex >= melodyLength) {
        currentNoteIndex = 0;
      }
    }
    beatCounter--;
  }
}

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
      } else {
        unsigned long duration = currentTime - previousSlitTime;
        previousSlitTime = currentTime;

        if (duration > 0 && duration < 2000000) {
          beatInterval = duration / 1000;
          currentBPM = (60000000.0 / ((float)duration * beatcount * 2));

          // スリット検知ごとにBPM表示
          Serial.print("BPM:");
          Serial.println(currentBPM);

          slitCount++;
          if (slitCount >= beatcount) {
            slitCount = 0;

            unsigned long now = micros();
            Serial.println("================================");
            Serial.print(">>> [拍検知!] t=");
            Serial.print(now - startTime);  // ★起動基準の相対時刻
            Serial.print("us  BPM:");
            Serial.println(currentBPM);
            Serial.println("================================");
          }
          
          pulseDetected = true; 
        } else {
          previousSlitTime = currentTime;
        }
      }
    }
  }
  lastSensorState = currentSensorState;
}