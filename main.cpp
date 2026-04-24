#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>

// ---Log関数 ---
void Log(const std::string& message) {
    OutputDebugStringA(message.c_str());
}

// ConvertString関数 ---
std::wstring ConvertString(const std::string& str) {
    auto size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring strTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed);
    return strTo;
}

std::string ConvertString(const std::wstring& str) {
    auto size_needed = WideCharToMultiByte(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &str[0], (int)str.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam) {
    // メッセージに応じてゲーム固有の処理を行う
    switch (msg) {
        // ウィンドウが破棄された
    case WM_DESTROY:
        // OSに対して、アプリの終了を伝える
        PostQuitMessage(0);
        return 0;
    }

    // 標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

    WNDCLASS wc{};
    // ウィンドウプロシージャ
    wc.lpfnWndProc = WindowProc;
    // ウィンドウクラス名(なんでも良い)
    wc.lpszClassName = L"CG2WindowClass";
    // インスタンスハンドル
    wc.hInstance = GetModuleHandle(nullptr);
    // カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // ウィンドウクラスを登録する
    RegisterClass(&wc);

    // クライアント領域のサイズ
    const int32_t kClientWidth = 1280;
    const int32_t kClientHeight = 720;
    
  

    // ウィンドウサイズを表す構造体にクライアント領域を入れる
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };

    // クライアント領域を元に実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウの生成
    HWND hwnd = CreateWindow(
        wc.lpszClassName,        // 利用するクラス名
        L"CG2",                  // タイトルバーの文字（何でも良い）
        WS_OVERLAPPEDWINDOW,     // よく見るウィンドウスタイル
        CW_USEDEFAULT,           // 表示X座標（Windowsに任せる）
        CW_USEDEFAULT,           // 表示Y座標（WindowsOSに任せる）
        wrc.right - wrc.left,    // ウィンドウ横幅
        wrc.bottom - wrc.top,    // ウィンドウ縦幅
        nullptr,                 // 親ウィンドウハンドル
        nullptr,                 // メニューハンドル
        wc.hInstance,            // インスタンスハンドル
        nullptr);                // オプション

    // ウィンドウを表示する
    ShowWindow(hwnd, SW_SHOW);

    // 1. std::string の基本的な使い方（半角のみを使用）
    std::string str0{ "STRING!!!" };
    std::string str1{ std::to_string(10) };

    // 2. 変数の準備
    int enemyHp = 150;
    std::string texturePath = "resources/player.png";

    // 3. 自作の Log 関数を使って出力
    Log("Hello, DirectX!\n");
    Log(str0 + "\n");
    Log(str1 + "\n");

    // 4. std::format を使った実践的な出力
    // ※すべて半角英数で記述されています
    Log(std::format("enemyHp:{}, texturePath:{}\n", enemyHp, texturePath));

    MSG msg{};
    // ウィンドウの×ボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // ゲームの処理
        }
    }
    return 0;
}