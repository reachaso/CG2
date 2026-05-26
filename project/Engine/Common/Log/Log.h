#pragma once
#include <ostream>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>

/// @brief ログ出力および文字列変換を管理するクラス
class Log {
public:
  /// @brief 初期化処理。ログファイルのオープンなどを行う。
  static void Initialize();

  /// @brief 終了処理。ログファイルのクローズなどを行う。
  static void Finalize();

  /// @brief 指定した出力ストリームにメッセージを書き込む
  /// @param os 出力先のストリーム (std::cout や std::ofstream 等)
  /// @param message 書き込むメッセージ
  void WriteLog(std::ostream &os, const std::string &message);

  /// @brief 既定のログ出力先にメッセージを書き込む
  /// @param message 書き込むメッセージ
  void WriteLog(const std::string &message);

  /// @brief 標準出力およびログファイルにメッセージを出力する
  /// @param message 出力するメッセージ
  static void Print(const std::string &message);

  /// @brief マルチバイト文字列(string)をワイド文字列(wstring)に変換する
  /// @param str 変換元の文字列
  /// @return 変換後のワイド文字列
  std::wstring ConvertString(const std::string &str);

  /// @brief ワイド文字列(wstring)をマルチバイト文字列(string)に変換する
  /// @param wstr 変換元のワイド文字列
  /// @return 変換後のマルチバイト文字列
  std::string ConvertString(const std::wstring &wstr);

  /// @brief パスの区切り文字を '/' に統一する
  /// @param path 変換前のパス文字列
  /// @return 正規化されたパス文字列
  static std::string NormalizePath(const std::string &path);

  /// @brief エディタ表示用などに蓄積されたログ履歴を取得する
  static const std::vector<std::string>& GetHistory();

  /// @brief ログ履歴をクリアする
  static void ClearHistory();

private:
  static void AddHistory(const std::string& message);

private:
  static std::ofstream sLogFile_; ///< ログ出力用ファイルストリーム
  static std::vector<std::string> sHistory_; ///< メモリ上のログ履歴
  static std::mutex sHistoryMutex_; ///< 履歴操作の排他制御用ミューテックス
};
