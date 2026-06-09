// ==========================================
// 3. updatePID() : 回転速度安定化関数
// ==========================================
int updatePID(float targetBPM, float currentBPM) {
  float error = targetBPM - currentBPM;
  integral += error;
  float derivative = error - previous_error;

  float output = (Kp * error) + (Ki * integral) + (Kd * derivative);
  previous_error = error;

  int newPWM = currentPWM + (int)output;
  newPWM = constrain(newPWM, 0, 255);

  updatePWM(newPWM);
  return newPWM;
}