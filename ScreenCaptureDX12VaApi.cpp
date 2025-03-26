#include <iostream>
#include <string>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <chrono>
#include <thread>
#include <vfw.h>
#include "ScreenCaptureDX12VaApi.h"
#include <vector>
#include <fstream>
#include <wincodec.h>
#include <wrl/client.h>
using namespace Microsoft::WRL;
#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


CScreenCaptureDX12VaApi::CScreenCaptureDX12VaApi(void)
{
    D3D12SharedHandle = nullptr;

    d3d11FeatureLevel = D3D_FEATURE_LEVEL_11_0;
    isVideoProcEnabled = false;
    isFfmpegEnabled = false;
}

CScreenCaptureDX12VaApi::~CScreenCaptureDX12VaApi()
{

}

void CScreenCaptureDX12VaApi::CreateVASurfaces(void)
{
    int i = 0;
    VASurfaceAttrib createSurfacesAttribList1[3] = {};

    createSurfacesAttribList1[0].type = VASurfaceAttribExternalBufferDescriptor;
    createSurfacesAttribList1[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    createSurfacesAttribList1[0].value.type = VAGenericValueTypePointer;
    //createSurfacesAttribList1[0].value.value.p = D3D12SharedResource.GetAddressOf();
    createSurfacesAttribList1[0].value.value.p = D3D12SharedResource2;
    
    createSurfacesAttribList1[2].type = VASurfaceAttribPixelFormat;
    createSurfacesAttribList1[2].flags = VA_SURFACE_ATTRIB_SETTABLE;
    createSurfacesAttribList1[2].value.type = VAGenericValueTypeInteger;
    createSurfacesAttribList1[2].value.value.i = VA_FOURCC_NV12;

    createSurfacesAttribList1[1].type = VASurfaceAttribMemoryType;
    createSurfacesAttribList1[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    createSurfacesAttribList1[1].value.type = VAGenericValueTypeInteger;
    createSurfacesAttribList1[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_NTHANDLE;// VA_SURFACE_ATTRIB_MEM_TYPE_D3D12_RESOURCE;

    VAStatus va_status = vaCreateSurfaces(
        m_vaDisplay,
        VA_RT_FORMAT_YUV420, // TODO # no BGRA format??
        m_width,
        m_height,
        &vaSurfaces[0],
        1,
        createSurfacesAttribList1,
        _countof(createSurfacesAttribList1));
    if (va_status != VA_STATUS_SUCCESS)
    {
        std::cerr << "[FAIL] Failed to vaCreateSurfaces. va_status: " << std::hex << va_status << std::endl;
        return;
    }
    std::cout << "[ OK ] vaCreateSurfaces " << i << " successful" << std::hex << va_status << std::endl;


}

int CScreenCaptureDX12VaApi::InitializeVAAPI(void)
{
    DXGI_ADAPTER_DESC desc = {};
    hr = m_adapter->GetDesc(&desc);
    if (FAILED(hr))
    {
        std::cerr << "Failed to get desc from adapter. HRESULT: " << std::hex << hr << std::endl;
        return -1;
    }
    m_vaDisplay = vaGetDisplayWin32(&desc.AdapterLuid);
    if (!m_vaDisplay)
    {
        std::cerr << "vaGetDisplayWin32 failed to create a VADisplay." << std::endl;
        return -1;
    }

    int major_ver, minor_ver;
    VAStatus va_status = vaInitialize(m_vaDisplay, &major_ver, &minor_ver);
    if (va_status != VA_STATUS_SUCCESS)
    {
        std::cerr << "Failed to vaInitialize. va_status: " << std::hex << va_status << std::endl;
        return -1;
    }
    std::cout << "vaInitialize successful va_status: " << std::hex << va_status << std::endl;

    return 0;
}

void CScreenCaptureDX12VaApi::InitAdapter(void)
{
    UINT flags = DXGI_CREATE_FACTORY_DEBUG;
    CreateDXGIFactory2(flags, __uuidof(IDXGIFactory2), (void**)&m_factory);
    m_factory->EnumAdapters1(0, &m_adapter);
}

int CScreenCaptureDX12VaApi::DX11DesktopDuplication(void)
{

    InitAdapter();

    D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1
    };

    // Initialize Direct3D 11
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &d3d11Device, &d3d11FeatureLevel, &d3d11Context);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device.\n";
        return -1;
    }

    ComPtr<IDXGIOutput> output;
    m_adapter->EnumOutputs(0, &output);

    ComPtr<IDXGIOutput1> output1;
    output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);

    output1->DuplicateOutput(d3d11Device.Get(), &duplication);

    ID3D11Debug* debugLayer = nullptr;
    HRESULT hr = d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debugLayer);
    if (SUCCEEDED(hr)) {
        debugLayer->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
    }

    hr = D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device));
    if (FAILED(hr)) {
        std::cerr << "[FAIL] Failed to create D3D12 device.\n";
        return -1;
    }
    std::cout << "[ OK ] Successfully created D3D12 device.\n";

    hr = d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    if (FAILED(hr)) {
        std::cerr << "[FAIL] Failed to create ID3D12CommandAllocator. HRESULT: " << std::hex << hr << std::endl;
        return -1;
    }
    std::cout << "[ OK ] Successfully created ID3D12CommandAllocator.\n";

    hr = d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) {
        std::cerr << "[FAIL] Failed to create ID3D12CreateCommandList. HRESULT: " << std::hex << hr << std::endl;
        return -1;
    }
    std::cout << "[ OK ] Successfully created ID3D12CreateCommandList.\n";

    DX12CreateSharedRenderTargets();
    InitializeVAAPI();

    return 0;
}

int CScreenCaptureDX12VaApi::DX12CreateSharedRenderTargets(void)
{
    for (int i = 0; i < FrameCount; ++i) {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        desc.DepthOrArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        hr = d3d12Device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_SHARED,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_renderTargets[i])
        );
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create render target resource.");
        }

        std::wstring handleName = L"D3D12SharedHandle_" + std::to_wstring(i);
        hr = d3d12Device->CreateSharedHandle(m_renderTargets[i].Get(), nullptr, GENERIC_ALL, handleName.c_str(), &m_renderSharedHandles[i]);
        if (FAILED(hr)) {
            std::cerr << "Failed to Create D3D12SharedHandle" << std::hex << hr << std::endl;
            return -6;
        }
        std::cout << "[ OK ] Successfully created D3D12SharedHandle. " << handleName.c_str() << std::endl;;

        hr = d3d12Device->OpenSharedHandle(m_renderSharedHandles[i], IID_PPV_ARGS(&m_renderTargets2[i]));
        if (FAILED(hr)) {
            std::cerr << "Failed to Open D3D12SharedHandle" << std::hex << hr << std::endl;
            return -7;
        }
        std::cout << "[ OK ] Successfully created renderSharedHandles: " << i << " " << std::hex << hr << std::endl;

    }
    return 0;
}

void CScreenCaptureDX12VaApi::DX11CleanDesktopResource(void)
{
    desktopResource.Reset();
    if (!desktopResource) duplication->ReleaseFrame();
}

int CScreenCaptureDX12VaApi::DX11AquireD12Resource(void)
{
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC readbackDesc = {};
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.MipLevels = 1;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    readbackDesc.Width = inputDesc.Width;
    readbackDesc.Height = inputDesc.Height;
    readbackDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        
    hr = d3d12Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_SHARED,
        &readbackDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&readbackResource)
    );
    if (FAILED(hr)) {
        std::cerr << "[FAIL] Failed to CreateCommittedResource: " << std::hex << hr << std::endl;
        return -5;
    }
    std::cout << "[ OK ] Successfully created CreateCommittedResource: " << std::endl;

    // released previous handles before creating new ones
    if (D3D12SharedHandle) {
        CloseHandle(D3D12SharedHandle);
        D3D12SharedHandle = nullptr;
        std::cout << "[ OK ] Successfully closed D3D12SharedHandle: " << "D3D12SharedHandle" << std::endl;
    }

    hr = d3d12Device->CreateSharedHandle(readbackResource.Get(), nullptr, GENERIC_ALL, L"D3D12SharedHandle", &D3D12SharedHandle);
    if (FAILED(hr)) {
        std::cerr << "[FAIL] Failed to Create D3D12SharedHandle" << std::hex << hr << std::endl;
        return -6;
    }
    std::cout << "[ OK ] Successfully created D3D12SharedHandle: " << "D3D12SharedHandle" << std::endl;

    hr = d3d12Device->OpenSharedHandle(D3D12SharedHandle, IID_PPV_ARGS(&D3D12SharedResource2));
    if (FAILED(hr)) {
        std::cerr << "[FAIL] Failed to Open D3D12SharedHandle" << std::hex << hr << std::endl;
        return -7;
    }
    std::cout << "[ OK ] Successfully Opened D3D12SharedHandle: " << "D3D12SharedHandle" << std::endl;

    return 0;
}

int CScreenCaptureDX12VaApi::DX11ProcessScreenCapture(void)
{
    for (int frameCount = 0; frameCount < 10; ++frameCount) {
        // Acquire frame
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        hr = duplication->AcquireNextFrame(
            500,                            // Timeout in milliseconds
            &frameInfo,                     // Frame information structure
            desktopResource.GetAddressOf()  // Acquired desktop resource
        );
        if (FAILED(hr)) {
            std::cerr << "Failed to acquire frame: " << std::hex << hr << std::endl;
            return -1;
        }
        if (!desktopResource) {
            std::cerr << "desktopResource is null!" << std::endl;
            return -1;
        }

        ComPtr<ID3D11Texture2D> capturedTexture;
        desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&capturedTexture);
        if (FAILED(hr)) {
            std::cerr << "Failed to query ID3D11Texture2D interface from desktopResource. Error: " << std::hex << hr << "\n";
            DX11CleanDesktopResource();
            continue;
        }

        // Copy captured frame to input texture GPU->GPU copying
        d3d11Context->CopyResource(inputTexture.Get(), capturedTexture.Get());

        IDXGIResource* dxgiResource = nullptr;
        inputTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
    
        if (0)
        {
            HANDLE sharedHandle;
            hr = dxgiResource->GetSharedHandle(&sharedHandle);
            dxgiResource->Release();

            hr = d3d12Device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(&D3D12SharedResource));
            if (FAILED(hr)) {
                std::cerr << "Failed to open shared handle. Error: " << std::hex << hr << "\n";
                DX11CleanDesktopResource();
            }
        }
       
        DX11AquireD12Resource();

        CreateVASurfaces();


        // Release the frame once done    #TODO: move this to appropriate location    
        DX11CleanDesktopResource();

        if (frameCount == 5)
        {
            writeD3D11TextureToFile(inputTexture, m_width, m_height);
        }

        std::cout << "Processed frame " << frameCount + 1 << std::endl;
    }

    return 0;
}

void CScreenCaptureDX12VaApi::DX11CloseDevice(void)
{

    // Release all resources
    // all of the resources are ComPtr smart pointer from the Windows Runtime Library(WRL)
    // automatically handles the reference
    if (isVideoProcEnabled)
    {
        videoProcessor->Release();
        vpEnumerator->Release();
        videoDevice->Release();
        videoContext->Release();
    }

    if (inputTexture) inputTexture.Reset();
    if (outputTexture) outputTexture.Reset();
    if (duplication) duplication.Reset();
    if (d3d11Context) d3d11Context.Reset();
    if (d3d11Device) d3d11Device.Reset();
}


int CScreenCaptureDX12VaApi::StartCaptureDX11New(void)
{
    DX11DesktopDuplication();
    DX11CreateTextures();
    DX11ProcessScreenCapture();
    DX11CloseDevice();

    for (int i = 0; i < FrameCount; ++i)
    {
        if (m_renderSharedHandles[i]) {
            CloseHandle(m_renderSharedHandles[i]);
            m_renderSharedHandles[i] = nullptr;
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    CScreenCaptureDX12VaApi sc;
    sc.StartCaptureDX11New();
    return 0;
}


