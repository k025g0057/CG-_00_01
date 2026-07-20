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
#include <dxgidebug.h>
#include <dxcapi.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <vector>
#include <numbers>
#include <sstream>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif

//libのリンク-- -
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")



struct Vector4 {
    float x;
    float y;
    float z;
    float w;
};

// 3次元ベクトル
struct Vector3 {
    float x;
    float y;
    float z;
};

// 2次元ベクトル（UV座標用など）
struct Vector2 {
    float x;
    float y;
};

// Transform構造体
struct Transform {
    Vector3 scale;     // 拡大縮小
    Vector3 rotate;    // 回転（ラジアン）
    Vector3 translate; // 平行移動
};

struct Matrix4x4 {
    float m[4][4];
};

struct Matrix3x3 {
    float m[3][3];
};



// --- 追加：マテリアルの構造体定義 ---
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};



struct TransformationMatrix {
    Matrix4x4 wvp;
    Matrix4x4 World;
};


// 頂点データ構造体
struct VertexData {
    Vector4 position;
    Vector2 texcoord; 
    Vector3 normal;
};

struct DirectionalLight {
    Vector4 color;       //!< ライトの色
    Vector3 direction;   //!< ライトの向き
    float intensity;     //!< 輝度
};


// 1. 行列の加法
Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

// 2. 行列の減法
Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);

// 3. 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

// 4. 逆行列
Matrix4x4 Inverse(const Matrix4x4& m);

// 5. 転置行列
Matrix4x4 Transpose(const Matrix4x4& m);

// 6. 単位行列の作成
Matrix4x4 MakeIdentity4x4();


// 1. 行列の加法
Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = m1.m[i][j] + m2.m[i][j];
        }
    }
    return result;
}

// 2. 行列の減法
Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = m1.m[i][j] - m2.m[i][j];
        }
    }
    return result;
}

// 3. 行列の積 
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result = { 0 }; // 0で初期化
    for (int i = 0; i < 4; ++i) { // 左の行
        for (int j = 0; j < 4; ++j) { // 右の列
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += m1.m[i][k] * m2.m[k][j];
            }
        }
    }
    return result;
}

// 4. 逆行列
Matrix4x4 Inverse(const Matrix4x4& m) {
    float a[4][8]; // 元の行列と単位行列をくっつけた作業用行列
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            a[i][j] = m.m[i][j];
            a[i][j + 4] = (i == j) ? 1.0f : 0.0f;
        }
    }

    for (int i = 0; i < 4; ++i) {
        // ピボット選択
        int pivot = i;
        for (int j = i + 1; j < 4; ++j) {
            if (std::abs(a[j][i]) > std::abs(a[pivot][i])) pivot = j;
        }
        std::swap(a[i], a[pivot]);

        // 行を正規化
        float temp = a[i][i];
        if (std::abs(temp) < 1e-6f) return MakeIdentity4x4(); // エラー時は単位行列を返す

        for (int j = 0; j < 8; ++j) a[i][j] /= temp;

        // 他の行を掃き出し
        for (int j = 0; j < 4; ++j) {
            if (i != j) {
                float factor = a[j][i];
                for (int k = 0; k < 8; ++k) a[j][k] -= factor * a[i][k];
            }
        }
    }

    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a[i][j + 4];
        }
    }
    return result;
}

// 5. 転置行列 (行と列を入れ替える)
Matrix4x4 Transpose(const Matrix4x4& m) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = m.m[j][i];
        }
    }
    return result;
}

// 6. 単位行列の作成
Matrix4x4 MakeIdentity4x4() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

//拡大縮小行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
    return {
        scale.x, 0.0f,    0.0f,    0.0f,
        0.0f,    scale.y, 0.0f,    0.0f,
        0.0f,    0.0f,    scale.z, 0.0f,
        0.0f,    0.0f,    0.0f,    1.0f
    };
}

// 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        translate.x, translate.y, translate.z, 1.0f
    };
}


// X軸回転行列の実装
Matrix4x4 MakeRotateXMatrix(float radian) {
    float s = std::sin(radian);
    float c = std::cos(radian);
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, c,    s,    0.0f,
        0.0f, -s,   c,    0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

// Y軸回転行列の実装
Matrix4x4 MakeRotateYMatrix(float radian) {
    float s = std::sin(radian);
    float c = std::cos(radian);
    return {
        c,    0.0f, -s,   0.0f, // ここが -s
        0.0f, 1.0f, 0.0f, 0.0f,
        s,    0.0f, c,    0.0f, // ここが s
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

// Z軸回転行列の実装
Matrix4x4 MakeRotateZMatrix(float radian) {
    float s = std::sin(radian);
    float c = std::cos(radian);
    return {
        c,    s,    0.0f, 0.0f,
        -s,   c,    0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

// 3次元アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
    Matrix4x4 sMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rXMatrix = MakeRotateXMatrix(rotate.x);
    Matrix4x4 rYMatrix = MakeRotateYMatrix(rotate.y);
    Matrix4x4 rZMatrix = MakeRotateZMatrix(rotate.z);
    Matrix4x4 tMatrix = MakeTranslateMatrix(translate);

    // 回転行列を合成 (X * Y * Z)
    Matrix4x4 rXYZMatrix = Multiply(rXMatrix, Multiply(rYMatrix, rZMatrix));

    // 全体を合成 (S * R * T)
    return Multiply(sMatrix, Multiply(rXYZMatrix, tMatrix));
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ) {
    float h = 1.0f / std::tan(fovY / 2.0f);
    float w = h / aspect;
    return Matrix4x4{
        w, 0, 0, 0,
        0, h, 0, 0,
        0, 0, farZ / (farZ - nearZ), 1,
        0, 0, (-nearZ * farZ) / (farZ - nearZ), 0
    };
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
    Matrix4x4 result = {};
    result.m[0][0] = 2.0f / (right - left);
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[2][2] = 1.0f / (farClip - nearClip);
    result.m[3][0] = (left + right) / (left - right);
    result.m[3][1] = (top + bottom) / (bottom - top);
    result.m[3][2] = nearClip / (nearClip - farClip);
    result.m[3][3] = 1.0f;
    return result;
}

struct ModelData {
    std::vector<VertexData> vertices;
};

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    // 1. 中で必要となる変数の宣言
    ModelData modelData; // 構築するModelData
    std::vector<Vector4> positions; // 位置
    std::vector<Vector3> normals; // 法線
    std::vector<Vector2> texcoords; // テクスチャ座標
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2. ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 3. 実際にファイルを読み、ModelDataを構築していく
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; // 先頭の識別子を読む

        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        }
        else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        }
        else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (identifier == "f") {
            // 面は三角形限定。その他は未対応
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                // 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(v, index, '/');// /区切りでインデックスを読んでいく
                    elementIndices[element] = std::stoi(index);
                }
                // 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];
                VertexData vertex = { position, texcoord, normal };
                modelData.vertices.push_back(vertex);
            }
        }
    }

    // 4. ModelDataを返す
    return modelData;
}

// --- スライド「CrashHandlerの登録」: 関数定義 ---
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力。
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


// ファイルと出力ウィンドウ両方に出す関数 ---
void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}

void Log(std::ostream& os, const std::wstring& message) {
    std::string str = ConvertString(message);
    os << str << std::endl;
    OutputDebugStringA(str.c_str());
}





// --- 「DescriptorHeap作成の関数化」: WinMainより上で定義 ---
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

// 
ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
    // 1. リソース用のヒープの設定
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

    // 2. リソースの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes; // 引数で受け取ったサイズを指定
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 3. 実際にリソースを作る
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

//指定インデックスのCPUディスクリプタハンドルを取得する
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

// 指定インデックスのGPUディスクリプタハンドルを取得する 
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}

IDxcBlob* CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring& filePath,
    // Compilerに使用するProfile
    const wchar_t* profile,
    // 初期化で生成したものを3つ
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler,
    std::ostream& logStream)
{
    // -------------------------------------------------------
    // 1. PSHLSLファイルを読む
    // -------------------------------------------------------
    // これからシェーダーをコンパイルする旨をログに出す
    Log(logStream, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

    // PSHLSLファイルを読み込む
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    // 読めなかったら止める
    assert(SUCCEEDED(hr));

    // 読み込んだファイルの内容を設定する
    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

    // -------------------------------------------------------
    // 2. Compileする
    // -------------------------------------------------------
    LPCWSTR arguments[] = {
        filePath.c_str(),         // コンパイル対象のPSHLSLファイル名
        L"-E", L"main",           // エントリーポイントの指定。基本的にmain以外にはしない
        L"-T", profile,           // ShaderProfileの設定
        L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
        L"-Od",                   // 最適化を外しておく
        L"-Zpr",                  // メモリレイアウトは行優先
    };

    // 実際にShaderをコンパイルする
    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler->Compile(
        &shaderSourceBuffer,    // 読み込んだファイル
        arguments,              // コンパイルオプション
        _countof(arguments),    // コンパイルオプションの数
        includeHandler,         // includeが含まれた諸々
        IID_PPV_ARGS(&shaderResult) // コンパイル結果
    );
    // コンパイルエラーではなくdxcが起動できないなど致命的な状況
    assert(SUCCEEDED(hr));

    // -------------------------------------------------------
    // 3. 警告・エラーがでていないか確認する
    // -------------------------------------------------------
    // 警告・エラーが出ていたらログに出して止める
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(logStream, shaderError->GetStringPointer());
        // 警告・エラーダメゼッタイ
        assert(false);
    }

    // -------------------------------------------------------
    // 4. Compile結果を受け取って返す
    // -------------------------------------------------------
    // コンパイル結果から実行用のバイナリ部分を取得
    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    // 成功したログを出す
    Log(logStream, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

    // もう使わないリソースを解放
    shaderSource->Release();
    shaderResult->Release();

    // 実行用のバイナリを返却
    return shaderBlob;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
#endif
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

// テクスチャデータをファイルから読み込む関数
DirectX::ScratchImage LoadTexture(const std::string& filePath) {
    // 1. ファイルパスをワイド文字列に変換する
    std::wstring filePathW = ConvertString(filePath);

    // 2. WICテクスチャをロードする
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    assert(SUCCEEDED(hr));

    // 3. ミップマップの生成
    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::TEX_FILTER_DEFAULT, 4, mipImages);
    assert(SUCCEEDED(hr));

    // 4. ミップマップ付きのデータを返す
    return mipImages;
}



ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
    // metadataを基にResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height); // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。普段使っているのは2次元

 

   
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // スライドのコード

    // Resourceの生成
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの設定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される設定 ★ここが変わりました
        nullptr, // Clear最適値。使わないのでnullptr
        IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));

    return resource;
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width; // Textureの幅
    resourceDesc.Height = height; // Textureの高さ
    resourceDesc.MipLevels = 1; // mipmapの数
    resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使うフラグ

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作成

    // 深度値のクリア最適化設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマットを合わせる
    depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(一番奥)でクリア
    depthClearValue.DepthStencil.Stencil = 0; // 0でクリア

    // Resourceの生成
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態
        &depthClearValue, // クリア最適値
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));

    return resource;
}

// データを転送するUploadTextureData関数を作る
[[nodiscard]]
ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList) {
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
    uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
    ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);
    UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

    // Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList->ResourceBarrier(1, &barrier);

    return intermediateResource;
}


// Transform変数を作る
Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    SetUnhandledExceptionFilter(ExportDump);

    CoInitializeEx(0, COINIT_MULTITHREADED);

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

#ifdef _DEBUG
    ID3D12Debug1* debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバッグレイヤーを有効化する
        debugController->EnableDebugLayer();
        // さらにGPU側でもチェックを行うようにする
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

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

    // SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
    ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);


    // DescriptorSizeを取得しておく
    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

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

    // 1つ目 (インデックス 0 番目)
    rtvHandles[0] = GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, 0);
    device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);

    // 2つ目 (インデックス 1 番目)
    rtvHandles[1] = GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, 1);
    device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

    Log(logStream, "Complete create RTVs!!!\n");

    
    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    // FenceのSignalを待つためのイベントを作成する
    HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);

    // dxcCompilerを初期化
    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    // 現時点でincludeはしないが、includeに対応するための設定を行っておく
    IDxcIncludeHandler* includeHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(SUCCEEDED(hr));

      // ルートシグネチャの設定
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
   

    // テクスチャ用のDescriptorRangeの設定
    D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
    descriptorRange[0].BaseShaderRegister = 0; // register(t0)の「0」
    descriptorRange[0].NumDescriptors = 1;     // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV（テクスチャ）用
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

   
    D3D12_ROOT_PARAMETER rootParameters[4]{};

    // [0] マテリアル用 (b0) の設定
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] WVP行列用 (b1) の設定
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
    rootParameters[1].Descriptor.ShaderRegister = 1;

    // [2] テクスチャ用 (t0) の設定を追加
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // テーブル構造
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;          // ピクセルシェーダーで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;        // 作成したレンジを紐付け
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // CBVを使う
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;   // PixelShaderで使う
    rootParameters[3].Descriptor.ShaderRegister = 1;                     // レジスタ番号1を使う

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 自動的に「4」になります

    // 6枚目のスライド：StaticSamplerの設定を追加
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ（綺麗に引き伸ばす）
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 画面外にでたらリピートする
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの制限なし
    staticSamplers[0].ShaderRegister = 0;         // register(s0)の「0」
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う

  

    // サンプラーをルートシグネチャに紐付けます
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;






    // 2. シリアライズ
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    // 3. 生成
    ID3D12RootSignature* rootSignature = nullptr; // ※型名のスペルミス(ID312...)に注意！
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    if (signatureBlob) signatureBlob->Release();
    if (errorBlob) errorBlob->Release();
    // ----------------------------------------------



   //
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    // ★ 法線の入力レイアウトを追加
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);





    // --- BlendStateの設定を行う ---
    D3D12_BLEND_DESC blendDesc{};
    // すべての色要素を書き込む
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


    // ---RasterizerStateの設定を行う ---
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    // 裏面（時計回り）を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


    // ---ShaderをCompileする内容 ---

    // VertexShaderをコンパイルする
    IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl",
        L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
    assert(vertexShaderBlob != nullptr);

    // PixelShaderをコンパイルする
    IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl",
        L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
    assert(pixelShaderBlob != nullptr);




    // --- PSOを生成する ---

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature; // RootSignature
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;  // InputLayout
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // VertexShader
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // PixelShader
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState


    // DepthStencilStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定をPSOに書き込む
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;



    // 書き込むRTVの情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // 利用するトポロジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // どのように画面に色を打ち込むかの設定（気にしなくて良い）
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 実際に生成
    ID3D12PipelineState* graphicsPipelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));

    ModelData modelData = LoadObjFile("resources", "plane.obj");

    // ★ 頂点リソースを作る
    ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

    // ★ 頂点バッファビューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    // ★ 頂点リソースにデータを書き込む
    VertexData* vertexData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());


    // Sprite用の頂点リソースを作る
    ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

    // 頂点バッファビューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
    // リソースの先頭のアドレスから使う
    vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
    // 使用するリソースのサイズは頂点6つ分のサイズ
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
    // 1頂点あたりのサイズ
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);


    // Resourceを作成する。1つあたりのIndexのサイズは32bitとするので、必要なサイズは32bit*6
    ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

    // Viewを作成する。Indexに対応したViewはIndexBufferView(IBV)である
    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
    // リソースの先頭のアドレスから使う
    indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
    // 使用するリソースのサイズはインデックス6つ分のサイズ
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    // インデックスはuint32_tとする
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;


    VertexData* vertexDataSprite = nullptr;
    vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
    vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 0: 左下
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
    vertexDataSprite[0].normal = { 0.0f, 0.0f, 1.0f };

    vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };   // 1: 左上
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
    vertexDataSprite[1].normal = { 0.0f, 0.0f, 1.0f };

    vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 2: 右下
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
    vertexDataSprite[2].normal = { 0.0f, 0.0f, 1.0f };

    vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };   // 3: 右上
    vertexDataSprite[3].texcoord = { 1.0f, 0.0f };
    vertexDataSprite[3].normal = { 0.0f, 0.0f, 1.0f };


    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));

    // 1枚目の三角形 (0, 1, 2)
    indexDataSprite[0] = 0;    indexDataSprite[1] = 1;    indexDataSprite[2] = 2;

    // 2枚目の三角形 (1, 3, 2)  ★ここが重要です！
    indexDataSprite[3] = 1;    indexDataSprite[4] = 3;    indexDataSprite[5] = 2;



    // --- MaterialResourceを生成する ---
// マテリアル用のリソース（CBuffer）を作る
    ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
    // マテリアルにデータを書き込むためのポインタ
    Material* materialData = nullptr;
    // 書き込むためのアドレスを取得（Map）
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // 今回は城（R=1.0, G=1.0, B=1.0, A=1.0）を設定してみる
    materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };

    materialData->enableLighting = true;

    materialData->uvTransform = MakeIdentity4x4();

    // ==========================================
    // Sprite用のマテリアルリソースを作る
    // ==========================================
    ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
    Material* materialDataSprite = nullptr;
    materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
    // 色は白を設定
    materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    // SpriteはLightingしないのでfalseを設定する
    materialDataSprite->enableLighting = false;

    materialDataSprite->uvTransform = MakeIdentity4x4();
    // ==========================================


    // ★サイズ TransformationMatrix
    ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
    // ★ポインタの型を TransformationMatrix* に変更
    TransformationMatrix* wvpData = nullptr;
    // 書き込むためのアドレスを取得
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    // ★構造体のWVPとWorldにそれぞれ単位行列を入れておく
    wvpData->wvp = MakeIdentity4x4();
    wvpData->World = MakeIdentity4x4();

    // 2. Transform変数を作る
    Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };



    // ★サイズを Matrix4x4 から TransformationMatrix に変更
    ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));
    // ★ポインタの型を TransformationMatrix* に変更
    TransformationMatrix* transformationMatrixDataSprite = nullptr;
    // 書き込むためのアドレスを取得
    transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
    // ★構造体のWVPとWorldにそれぞれ単位行列を入れておく
    transformationMatrixDataSprite->wvp = MakeIdentity4x4();
    transformationMatrixDataSprite->World = MakeIdentity4x4();



    Transform transformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    Vector3 cameraRotateDeg = { 0.0f, 0.0f, 0.0f };

    Transform uvTransformSprite{
    { 1.0f, 1.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f },
    };

    // ===================================================================
    // 平行光源用のResourceを作成し、値を設定する ＝
    // ===================================================================
    ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

    // デフォルト値の設定（真上から白いライトで照らす）
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData->intensity = 2.0f;
    // ===================================================================


    Transform cameraTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };

    Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

    // --- ViewportとScissor(シザー)の内容 ---

    // ビューポート
    D3D12_VIEWPORT viewport{};
    // クライアント領域のサイズと一緒にして画面全体に表示
    viewport.Width = kClientWidth;
    viewport.Height = kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // シザー矩形
    D3D12_RECT scissorRect{};
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = kClientHeight;

    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。
// こういうもんである
#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device,
        swapChainDesc.BufferCount,
        rtvDesc.Format,
        srvDescriptorHeap,
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();
#endif


#ifdef _DEBUG
    ID3D12InfoQueue* infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // ヤバイエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        // 抑制するメッセージのID
        D3D12_MESSAGE_ID denyIds[] = {
            /* Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ */
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);

        // 解放
        infoQueue->Release();
    }
#endif

    // Textureを読んで転送する
    DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    ID3D12Resource* textureResource = CreateTextureResource(device, metadata);

    // 指定された転送コード
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
    uint64_t intermediateSize = GetRequiredIntermediateSize(textureResource, 0, UINT(subresources.size()));
    ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);

    UpdateSubresources(commandList, textureResource, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

    // Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = textureResource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList->ResourceBarrier(1, &barrier);

    // 最後のスライドのコード
    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, commandLists);
    fenceValue++;
    commandQueue->Signal(fence, fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);

    intermediateResource->Release();
    // ===================================================================

    // DepthStencilTextureをウィンドウのサイズで作成
    ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);


    // DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
   // dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture// DSVHeapの先頭にDSVをつくる
    device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    // ===================================================================


    // metaDataを基にSRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    // ＝★ 変更: 作成した関数を使ってインデックス「1」番目のハンドルをスマートに取得 ★＝
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

    // SRVの生成
    device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

    //=====================================================================--
    //2枚目のテクスチャ読み込み
    //=======================================================================
   
    DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);

    // ＝★ 1枚目と同様の手動転送処理 ★＝
    std::vector<D3D12_SUBRESOURCE_DATA> subresources2;
    DirectX::PrepareUpload(device, mipImages2.GetImages(), mipImages2.GetImageCount(), mipImages2.GetMetadata(), subresources2);
    uint64_t intermediateSize2 = GetRequiredIntermediateSize(textureResource2, 0, UINT(subresources2.size()));
    ID3D12Resource* intermediateResource2 = CreateBufferResource(device, intermediateSize2);

    UpdateSubresources(commandList, textureResource2, intermediateResource2, 0, 0, UINT(subresources2.size()), subresources2.data());

    // ResourceStateをGENERIC_READに変更する
    D3D12_RESOURCE_BARRIER barrier2{};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier2.Transition.pResource = textureResource2;
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList->ResourceBarrier(1, &barrier2);

    // ＝★ 1枚目の直後と同様に、GPUへのコマンド実行を待って intermediateResource2 をReleaseする ★＝
    commandList->Close();
    ID3D12CommandList* commandLists2[] = { commandList };
    commandQueue->ExecuteCommandLists(1, commandLists2);
    fenceValue++;
    commandQueue->Signal(fence, fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);

    intermediateResource2->Release();


    // meataDataを基にSRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    // index=2の位置にDescriptorを生成する
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

    bool useMonsterBall = true;

    MSG msg{};
    // ウィンドウの×ボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        // Windowにメッセージが来てたら最優先で処理させる
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
#ifdef USE_IMGUI
            // imguiのフレーム開始
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            
            ImGui::NewFrame();

            // 開発用UIの処理。実際に開発用UIを出す場合はここをゲーム固有の処理に置き換える
            ImGui::ShowDemoWindow();

            ImGui::Begin("Settings");

            // 1. カメラコントロール
            ImGui::DragFloat3("CameraTranslate", &cameraTransform.translate.x, 0.05f);
            ImGui::SliderFloat("CameraRotateX", &cameraRotateDeg.x, -360.0f, 360.0f, "%.0f deg");
            ImGui::SliderFloat("CameraRotateY", &cameraRotateDeg.y, -360.0f, 360.0f, "%.0f deg");
            ImGui::SliderFloat("CameraRotateZ", &cameraRotateDeg.z, -360.0f, 360.0f, "%.0f deg");

            // 2. モデル（Sphere / Plane）の回転UIを追加
            static Vector3 modelRotateDeg = { 0.0f, 0.0f, 0.0f }; // 度数法での回転角
            ImGui::SliderFloat("SphereRotateX", &modelRotateDeg.x, -360.0f, 360.0f, "%.0f deg");
            ImGui::SliderFloat("SphereRotateY", &modelRotateDeg.y, -360.0f, 360.0f, "%.0f deg");
            ImGui::SliderFloat("SphereRotateZ", &modelRotateDeg.z, -360.0f, 360.0f, "%.0f deg");

            // 3. マテリアル設定
            ImGui::ColorEdit4("color", &materialData->color.x, ImGuiColorEditFlags_Uint8);

            // enableLightingトグル
            bool enableLightingBool = (materialData->enableLighting != 0);
            if (ImGui::Checkbox("enableLighting", &enableLightingBool)) {
                materialData->enableLighting = enableLightingBool ? 1 : 0;
            }

            // 4. スプライト設定
            ImGui::ColorEdit4("colorSprite", &materialDataSprite->color.x, ImGuiColorEditFlags_Uint8);
            ImGui::DragFloat3("translateSprite", &transformSprite.translate.x, 1.0f);

            // 5. テクスチャ切り替えトグル
            ImGui::Checkbox("useMonsterBall", &useMonsterBall);

            // 6. 平行光源（Directional Light）コントロール
            ImGui::ColorEdit3("LightColor", &directionalLightData->color.x, ImGuiColorEditFlags_Uint8);

            if (ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f, -1.0f, 1.0f)) {
                float length = std::sqrt(
                    directionalLightData->direction.x * directionalLightData->direction.x +
                    directionalLightData->direction.y * directionalLightData->direction.y +
                    directionalLightData->direction.z * directionalLightData->direction.z
                );
                if (length > 0.0f) {
                    directionalLightData->direction.x /= length;
                    directionalLightData->direction.y /= length;
                    directionalLightData->direction.z /= length;
                }
            }
            ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);

            // 7. UV Transfrom コントロール
            ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
            ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
            ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);

            ImGui::End();

            // --- 度数法 (Deg) からラジアン (Rad) への変換計算 ---
            // カメラ回転の適用
            cameraTransform.rotate.x = cameraRotateDeg.x * (std::numbers::pi_v<float> / 180.0f);
            cameraTransform.rotate.y = cameraRotateDeg.y * (std::numbers::pi_v<float> / 180.0f);
            cameraTransform.rotate.z = cameraRotateDeg.z * (std::numbers::pi_v<float> / 180.0f);

            // モデル回転の適用（SphereRotate の値を transform に反映）
            transform.rotate.x = modelRotateDeg.x * (std::numbers::pi_v<float> / 180.0f);
            transform.rotate.y = modelRotateDeg.y * (std::numbers::pi_v<float> / 180.0f);
            transform.rotate.z = modelRotateDeg.z * (std::numbers::pi_v<float> / 180.0f);

#endif
            // 固定データ・ゲームの更新処理
            //transform.rotate.y += 0.03f; // 毎フレーム回転させる

            // 1. 各種行列の計算合成 (World -> View -> Projection)
            Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
            Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
            Matrix4x4 viewMatrix = Inverse(cameraMatrix);


            // 2. WVP行列を正しく合成してGPUに書き込む
            wvpData->wvp = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix)); // ★.WVP に代入
            wvpData->World = worldMatrix; // ★CPU側でWorldMatrixを設定


            // SpriteのTransformからWorldMatrixを作る
            Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
            // 2DなのでViewProjeccetionは単位行列でよい
            Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
            Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
            // WVPを作成
            Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
            // データを転送する
            transformationMatrixDataSprite->wvp = worldViewProjectionMatrixSprite; // ★.WVP に代入
            transformationMatrixDataSprite->World = worldMatrixSprite; // ★CPU側でWorldMatrixを設定


            // これから書き込むバックバッファのインデックスを取得
            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();


            D3D12_RESOURCE_BARRIER barrier{};
            // 今回のバリアはTransition
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            // Noneにしておく
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            // バリアを張る対象のリソース。現在のバックバッファに対して行う
            barrier.Transition.pResource = swapChainResources[backBufferIndex];
            // 遷移前（現在）のResourceState
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            // 遷移後のResourceState
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            // TransitionBarrierを張る
            commandList->ResourceBarrier(1, &barrier);


           

            // 指定した色で画面全体をクリアする
            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // 青っぽい色
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);


            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


            // 描画用のDescriptorHeapの設定
            ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);


            // --- コマンドを積むの内容 ---

            commandList->RSSetViewports(1, &viewport); // Viewportを設定
            commandList->RSSetScissorRects(1, &scissorRect); // Scissorを設定

            // RootSignatureを設定。PSOに設定しているけど別途設定が必要
            commandList->SetGraphicsRootSignature(rootSignature);

            //マテリアルCBufferの位置を設定
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());


            commandList->SetPipelineState(graphicsPipelineState); // PSOを設定

            commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定

            // 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // wvp用のCBufferの場所を設定（スロット1）
            commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

            commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

            // SRV用のDescriptorHeapをコマンドリストにセット
            ID3D12DescriptorHeap* pHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

            // ★3つ目のコードを適用（変数名を現在のコードに合わせて最適化しています）
         
            commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

#ifdef USE_IMGUI
            // ImGuiの内部コマンドを生成する
            ImGui::Render();
#endif

            // 描画！（DrawCall/ドローコール）
            commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

            commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());

            // ＝★ ②こちらはSprite用のまま「6」で残しておきます！ ★＝
            commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);

            commandList->IASetIndexBuffer(&indexBufferViewSprite); // IBVを設定

            commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

            // TransformationMatrixCBufferの場所を設定
            commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
            // 描画！（DrawCall／ドローコール）
            //commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

#ifdef USE_IMGUI
            // 実際のcommandListのImGuiの描画コマンドを積む
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif


            // 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
            // 今回はRenderTargetからPresentにする
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            // TransitionBarrierを張る
            commandList->ResourceBarrier(1, &barrier);

            // コマンドリストの内容を確定させる
            hr = commandList->Close();
            assert(SUCCEEDED(hr));

            // GPUにコマンドリストの実行を行わせる
            ID3D12CommandList* commandLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, commandLists);

            // GPUとOSに画面の交換を行うよう通知する
            swapChain->Present(1, 0);

            // Fenceの値を更新
            fenceValue++;
            // GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
            commandQueue->Signal(fence, fenceValue);

            // Fenceの値が指定したSignal値にたどり着いているか確認する
            // GetCompletedValueの初期値はFence作成時に渡した初期値
            if (fence->GetCompletedValue() < fenceValue)
            {
                // 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                // イベント待つ
                WaitForSingleObject(fenceEvent, INFINITE);
            }


            // 次のフレーム用のコマンドリストを準備
            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator, nullptr);
            assert(SUCCEEDED(hr));
        }
    }

    //↓AIが追加したほうがファイルに目印ができてわかりやすいと言ってたから。いつ消しても害なしーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    Log(logStream, "Application Ended");

#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    // 解放処理
  // [元からある解放群]
    vertexResource->Release();
    graphicsPipelineState->Release();
    materialResource->Release();
    textureResource->Release();
    materialResourceSprite->Release();

    if (textureResource2) {
        textureResource2->Release();
    }

    if (depthStencilResource) {
        depthStencilResource->Release();
    }

    // 行列リソースの解放
    if (wvpResource) { wvpResource->Release(); }

    if (directionalLightResource) { directionalLightResource->Release(); }

    if (srvDescriptorHeap) {
        srvDescriptorHeap->Release();
    }

    if (errorBlob) { errorBlob->Release(); }
    rootSignature->Release();
    pixelShaderBlob->Release();
    vertexShaderBlob->Release();

    //  DXC関連の解放
    if (includeHandler) { includeHandler->Release(); }
    if (dxcCompiler) { dxcCompiler->Release(); }
    if (dxcUtils) { dxcUtils->Release(); }

    CloseHandle(fenceEvent);
    fence->Release();
    rtvDescriptorHeap->Release();
     dsvDescriptorHeap->Release();
  
     if (vertexResourceSprite) { vertexResourceSprite->Release(); }

     if (indexResourceSprite) { indexResourceSprite->Release(); }
    

    for (int i = 0; i < 2; ++i) {
        swapChainResources[i]->Release();
    }

    if (transformationMatrixResourceSprite) {
        transformationMatrixResourceSprite->Release();
    }

    swapChain->Release();
    commandList->Release();
    commandAllocator->Release();
    commandQueue->Release();

    // ★追加: アダプターとファクトリーの解放
    if (useAdapter) { useAdapter->Release(); }
    if (dxgiFactory) { dxgiFactory->Release(); }

    

    // 最後にデバイスを解放
    device->Release();

    CoUninitialize();

    IDXGIDebug1* debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        debug->Release();
    }
    
    return 0;
}