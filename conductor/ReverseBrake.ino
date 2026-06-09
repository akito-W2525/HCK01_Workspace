// ==========================================
// 4. ReverseBrake() : 停止時ブレーキ関数
// ==========================================
void ReverseBrake() {
  // 1. 逆相ブレーキ（一瞬だけ逆転）
  digitalWrite(MOTOR_IN1, LOW);
  analogWrite(MOTOR_IN2, 255);

  // 慣性を打ち消すための微小時間 (要調整)
  delay(50);

  // 2. ショートブレーキ（モーターの両端子をHIGHにして固定）
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, HIGH);

  currentPWM = 0;
}