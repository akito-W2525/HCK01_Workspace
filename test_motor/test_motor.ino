int Motor01_in1 = 0; // 0番ピンから4番ピンへ変更を推奨
int Motor01_in2 = 1; // 1番ピンから5番ピンへ変更を推奨
int PWM = 3;

#define IRRCV_PIN A0

void setup() {
  pinMode(Motor01_in1, OUTPUT);
  pinMode(Motor01_in2, OUTPUT);
  pinMode(PWM, OUTPUT); // PWMピンも念のためOUTPUT設定にする

  Serial.begin(115200);
  pinMode(IRRCV_PIN, INPUT);

}

void loop() {
  int val = analogRead(IRRCV_PIN);
  Serial.println(val);
}








// 3. updatePID() : 回転速度安定化関数
// 役割: 目標BPMと実測BPMの誤差からPID成分を算出し、PWM値を最適化
int updatePID(float targetBPM, float currentBPM) {
  float error = targetBPM - currentBPM;
  integral += error;
  float derivative = error - previous_error;

  float output = (Kp * error) + (Ki * integral) + (Kd * derivative);
  previous_error = error;

  // 現在のPWM値にPIDの補正出力を加算
  int newPWM = currentPWM + (int)output;
  newPWM = constrain(newPWM, 0, 255);

  updatePWM(newPWM); // 新しいPWM値でモーターを更新
  return newPWM;
}


// 4. ReverseBrake() : 停止時ブレーキ関数
// 役割: 逆相ブレーキをかけ、直後にショートブレーキ状態へ移行
void ReverseBrake() {
  // 1. PWM値最大で一瞬だけ「逆転」させる（逆相ブレーキ）
  digitalWrite(MOTOR_IN1, LOW);
  analogWrite(MOTOR_IN2, 255);
  
  // 慣性に合わせて微小時間を調整
  delay(50); 

  // 2. 設定時間経過後、ショートブレーキ状態へ (TA7291P: IN1=HIGH, IN2=HIGH)
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, HIGH);

  currentPWM = 0; // PWM値をリセット
}



