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
#include <format>

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

// --- スライド「DescriptorHeap作成の関数化」: WinMainより上で定義 ---
ID3D12DescriptorHeap* CreateDescriptorHeap(
    ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
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

    // 1. std::string の基本的な使い方（半角のみを使用）
    std::string str0{ "STRING!!!" };
    std::string str1{ std::to_string(10) };

    // 2. 変数の準備
    int enemyHp = 150;
    std::string texturePath = "resources/player.png";

    // 3. 自作の Log 関数を使って出力
    Log(logStream, "Hello, DirectX!\n");
    Log(logStream, str0 + "\n");
    Log(logStream, str1 + "\n");

    // 4. std::format を使った実践的な出力
    // ※すべて半角英数で記述されています
    Log(logStream, std::format("enemyHp:{}, texturePath:{}\n", enemyHp, texturePath));

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


    // ---「D3D12Deviceの生成」 ---
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


    // --- スライドCG01_00の６．７スライドーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー ---
    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
    // コマンドキューの生成がうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));
    Log(logStream, "Complete create D3D12CommandQueue!!!\n");

    // --- スライド「コマンドアロケータを生成する」 ---
    ID3D12CommandAllocator* commandAllocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    // コマンドアロケータの生成がうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));
    Log(logStream, "Complete create D3D12CommandAllocator!!!\n");

    // --- スライド「コマンドリストを生成する」 ---
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
    // コマンドリストの生成がうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));
    Log(logStream, "Complete create D3D12GraphicsCommandList!!!\n");
    //ここまでーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    // --- スライド「スワップチェーンを生成する」 ---
    IDXGISwapChain4* swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = kClientWidth;     // 画面の幅。ウィンドウのクライアント領域を同じものにする
    swapChainDesc.Height = kClientHeight;   // 画面の高さ。ウィンドウのクライアント領域を同じものにする
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // 色の形式
    swapChainDesc.SampleDesc.Count = 1;     // マルチサンプルしない
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして使う
    swapChainDesc.BufferCount = 2;          // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタに移したら、中身を破棄
    // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
    hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
    assert(SUCCEEDED(hr));
    Log(logStream, "Complete create DXGISwapChain4!!!\n");

    // --- スライド「RTV用ディスクリプタヒープの生成」 ---
    // ディスクリプタヒープの生成 (定義順を修正)
    ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    Log(logStream, "Complete create RTV DescriptorHeap!!!\n");

    ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    Log(logStream, "Complete create DSV DescriptorHeap!!!\n");

    // --- スライド「SwapChainからResourceを持ってくる」 ---
    ID3D12Resource* swapChainResources[2] = { nullptr };
    for (UINT i = 0; i < 2; ++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
        // これが取得できないのは一大事
        assert(SUCCEEDED(hr));
    }

    // --- スライド「RTVを作ろう」 ---
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして作成

    // ディスクリプタの先頭を取得
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // RTVを2つ作るのでディスクリプタハンドルも2つ
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

    // 1つ目
    rtvHandles[0] = rtvStartHandle;
    device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);

    // 2つ目（1つ目の後ろに作る）
    rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

    Log(logStream, "Complete create RTVs!!!\n");



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

            // これから書き込むバックバッファのインデックスを取得
            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

            // 描画先のRTVを設定する
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

            // 指定した色で画面全体をクリアする
            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // 青っぽい色
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

            // コマンドリストの内容を確定させる
            hr = commandList->Close();
            assert(SUCCEEDED(hr));

            // GPUにコマンドリストの実行を行わせる
            ID3D12CommandList* commandLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, commandLists);

            // GPUとOSに画面の交換を行うよう通知する
            swapChain->Present(1, 0);

            // 次のフレーム用のコマンドリストを準備
            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator, nullptr);
            assert(SUCCEEDED(hr));
        }
    }

    //↓AIが追加したほうがファイルに目印ができてわかりやすいと言ってたから。いつ消しても害なしーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    Log(logStream, "Application Ended");

    return 0;
}