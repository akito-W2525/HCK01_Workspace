// ==========================================
// 5. readTargetBPM() : 目標BPM読み取り関数
// ==========================================
// 引数: なし
// 戻り値: float（目標BPM値）
// 処理: 可変抵抗のアナログ値を読み取り，仕様であるBPM 120〜180の範囲に変換して返す
float readTargetBPM() {
  int rawValue = analogRead(POT_PIN); // 0 〜 1023 の値を取得

  // map関数を使って、0〜1023 の範囲を 120〜180 の範囲に比例変換する
  long mappedBPM = map(rawValue, 0, 1023, 120, 180);

  return (float)mappedBPM;
}