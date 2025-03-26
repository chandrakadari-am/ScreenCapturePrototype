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

void writeRGBAFrameToFile(const char* filename, uint8_t* rgbaBuffer, int width, int height)
{
    int size = width * height * 4;
    // Open the file in binary mode
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Unable to open file for writing." << std::endl;
        return;
    }

    // Write the RGBA buffer to the file
    file.write((const char*)rgbaBuffer, size);

    // Close the file
    file.close();
}


void CScreenCaptureDX12VaApi::writeD3D11TextureToFile(const ComPtr<ID3D11Texture2D>& tex, int width, int height)
{
    // Create a staging texture for CPU access
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    tex->GetDesc(&stagingDesc);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = d3d11Device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
    d3d11Context->CopyResource(stagingTexture.Get(), tex.Get());

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    hr = d3d11Context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);

    writeRGBAFrameToFile(".\\output.rgba", (uint8_t*)mappedResource.pData, m_width, m_height);

    d3d11Context->Unmap(stagingTexture.Get(), 0);

}

int CScreenCaptureDX12VaApi::DX11CreateTextures(void)
{
    // Input texture (captured screen)
    inputDesc = {};
    inputDesc.Width = m_width;
    inputDesc.Height = m_height;
    inputDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // Default screen format
    inputDesc.Usage = D3D11_USAGE_DEFAULT;
    //inputDesc.BindFlags = D3D11_BIND_RENDER_TARGET;// | D3D11_BIND_SHADER_RESOURCE;// D3D11_BIND_SHADER_RESOURCE;
    inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;// | D3D11_BIND_SHADER_RESOURCE;// D3D11_BIND_SHADER_RESOURCE;
    inputDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    inputDesc.MipLevels = 1;
    inputDesc.ArraySize = 1;
    inputDesc.SampleDesc.Count = 1;

    hr = d3d11Device->CreateTexture2D(&inputDesc, nullptr, &inputTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 texture.\n";
        return -1;
    }

    // Output texture (processed to NV12)
    outputDesc = inputDesc;
    outputDesc.Format = DXGI_FORMAT_NV12;
    outputDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    hr = d3d11Device->CreateTexture2D(&outputDesc, nullptr, &outputTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 texture.\n";
        return -1;
    }

    return 0;
}

