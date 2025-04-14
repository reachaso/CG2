#pragma warning(push)
#pragma warning(disable:28251)
#include <Windows.h>
#pragma warning(pop)

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");
	return 0;
}
