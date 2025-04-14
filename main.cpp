#pragma warning(push)
#pragma warning(disable:28251)
#include <Windows.h>
#include <cstdint>
#pragma warning(pop)

//クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

//ウィンドウサイズを表す構造体にクライアント領域を入れる
RECT wrc = { 0, 0, kClientWidth, kClientHeight };

//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam) {
    //メッセージに応じてゲーム固有の処理を行う
    switch (msg) {
        //ウィンドウが破棄された
    case WM_DESTROY:
        //OSに対して、アプリの終了を伝える
        PostQuitMessage(0);
        return 0;
    }

    //標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    
	WNDCLASS wc{};
    //ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
    //カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスの登録
	RegisterClass(&wc);

	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName, //ウィンドウクラス名
		L"CG2", //ウィンドウ名
		WS_OVERLAPPEDWINDOW, //ウィンドウスタイル
		CW_USEDEFAULT, //x座標
		CW_USEDEFAULT, //y座標
		wrc.right - wrc.left, //幅
		wrc.bottom - wrc.top, //高さ
		nullptr, //親ウィンドウハンドル
		nullptr, //メニューハンドル
		wc.hInstance, //インスタンスハンドル
		nullptr); //オプション

	MSG msg{};
	//ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		//メッセージがある場合は、メッセージを取得
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			//メッセージを処理
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			//ゲームの処理

			//ウィンドウの表示
			ShowWindow(hwnd, SW_SHOW);
		}
	}

	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");
	return 0;
}
