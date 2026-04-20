#include <Windows.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>   
#include <chrono>

// ファイルと出力ウィンドウ両方に出す関数 ---
void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
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

    // logsディレクトリを用意
    std::filesystem::create_directory("logs");

    // --- スライド「現在時刻でログファイル生成」 ---
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    std::ofstream logStream(logFilePath);

	//↓AIが追加したほうがファイルに目印ができてわかりやすいと言ってたからーいつ消しても害なしーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    Log(logStream, "Application Started");

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

    MSG msg{};
    // ウィンドウの×ボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        // Windowにメッセージが来てたら最優先で処理させる
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // ゲームの処理
        }
    }

   

     // 1. 文字列を格納する 
    std::string str0{ "STRING!!!" };

    // 2. 整数を文字列にする 
    std::string str1{ std::to_string(10) };

    // 出力ウィンドウへの文字出力
    OutputDebugStringA("Hello, DirectX!\n");

    // スライドで作った文字列を出力してみる（.c_str()を忘れずに！）
    OutputDebugStringA(str0.c_str());
    OutputDebugStringA(str1.c_str());

    //↓AIが追加したほうがファイルに目印ができてわかりやすいと言ってたから。いつ消しても害なしーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    Log(logStream, "Application Ended");



    return 0;
}