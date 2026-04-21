#include <Windows.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>   
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>

//libのリンク-- -
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")



// --- スライド「CrashHandlerの登録」: 関数定義 ---
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
    SYSTEMTIME time;
    GetLocalTime(&time);

    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);

    // StringCchPrintfWを利用してファイルパスを作成
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);

    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    // processId (このexeのId) とクラッシュ (例外) の発生したthreadIdを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    // 設定情報を入力
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;

    // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

    // 他に関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
    return EXCEPTION_EXECUTE_HANDLER;

}


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

    
    SetUnhandledExceptionFilter(ExportDump);

    uint32_t* p = nullptr; // 故意にクラッシュさせるためのポインた
    *p = 100; // 故意にクラッシュさせるためのコード

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

    // DXGIファクトリーの生成
    IDXGIFactory7* dxgiFactory = nullptr;

    // HRESULTはWindows系のエラーコードであり、関数が成功したかどうかをSUCCEEDEDマクロで判定できる
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

    // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));


    // ---「使用するアダプタ（GPU）を決定する」 ---
    // 使用するアダプタ用の変数。最初にnullptrを入れておく
    IDXGIAdapter4* useAdapter = nullptr;

    // 良い順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
        DXGI_ERROR_NOT_FOUND; ++i) {

        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);

        assert(SUCCEEDED(hr)); // 取得できないのは一大事

        // ソフトウェアアダプタでなければ採用！
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // 採用したアダプタの情報をログに出力。wstringの方なので注意
            Log(logStream, ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
            break;
        }
        useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }
    // 適切なアダプタが見つからなかったので起動できない
    assert(useAdapter != nullptr);


    // --- スライド「D3D12Deviceの生成」 ---
    ID3D12Device* device = nullptr;

    // 機能レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
    };

    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
        // 指定した機能レベルでデバイスが生成できたかを確認
        if (SUCCEEDED(hr)) {
            // 生成できたのでログ出力を行ってループを抜ける
            Log(logStream, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
            break;
        }
    }

    // デバイスの生成がうまくいかなかったので起動できない
    assert(device != nullptr);
    Log(logStream, "Complete create D3D12Device!!!\n"); // 初期化完了のログをだす

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