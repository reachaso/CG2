#pragma once

#include <functional>
#include <string>
#include <windows.h>

class Window {
public:
  // ウィンドウ生成時のコールバック型（メッセージ処理を外部で拡張したい場合に利用）
  using MsgHandler = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;

  Window(HINSTANCE hInst);
  ~Window();

  // ウィンドウを作成して表示する
  // title: ウィンドウタイトル
  // w, h: クライアント領域の幅・高さ
  bool Create(const std::wstring &title, int w, int h);

  // 毎フレーム呼び出して、WM_QUIT が来ていれば quit_ を true にする
  void PollMessages();

  // ウィンドウが閉じる準備（WM_QUIT）になったか
  bool ShouldClose() const { return quit_; }

  // ネイティブ HWND を取得
  HWND GetHandle() const { return hWnd_; }

  // メッセージ処理をカスタマイズしたいときにコールバックを登録
  void SetMsgHandler(MsgHandler handler) { customHandler_ = handler; }

private:
  // Win32 に登録するための静的プロシージャ。この中から this
  // ポインタを取り出してメンバに渡す
  static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);

  // 実際のメッセージ処理
  LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  HINSTANCE hInst_;
  HWND hWnd_;
  bool quit_;

  // ウィンドウサイズ
  int width_;
  int height_;

  // クラス登録で使う名前（UNICODE 版）
  std::wstring className_;

  // 外部から差し込みたいメッセージハンドラ
  MsgHandler customHandler_;
};
