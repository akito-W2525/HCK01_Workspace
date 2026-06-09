// ==========================================
// 2. updatePWM() : モーター出力制御関数
// ==========================================
void updatePWM(int pwmValue) {
  // 安全のためPWM値を0-255に制限
  pwmValue = constrain(pwmValue, 0, 255);

  // TA7291Pを用いた正転制御
  analogWrite(MOTOR_IN1, pwmValue);
  digitalWrite(MOTOR_IN2, LOW);

  currentPWM = pwmValue;
}