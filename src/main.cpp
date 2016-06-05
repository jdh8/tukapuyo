#include "GameAssets.h"
#include "gamemode.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	ChangeWindowMode(TRUE);
	SetAlwaysRunFlag(TRUE);

	// ＤＸライブラリ初期化処理
	if (DxLib_Init() == -1) 
		return -1;

	// 描画先を裏画面に
	SetDrawScreen(DX_SCREEN_BACK);

	GameAssets* s = new GameAssets;

	MainMenu::init(s);
	
	if (MainMenu::loop() == -1)
		return -1;
	
	DxLib_End();				

	return 0;
}
