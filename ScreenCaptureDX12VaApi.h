#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <iostream>
#include <va/va.h>
#include <va/va_win32.h>


using namespace Microsoft::WRL;
using Microsoft::WRL::ComPtr;

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class VAException : public std::runtime_error
{
public:
	VAException(VAStatus vas) : std::runtime_error(VAStatusToString(vas)), m_vas(vas) {}
	VAStatus Error() const { return m_vas; }
private:
	inline std::string VAStatusToString(VAStatus vas)
	{
		char s_str[64] = {};
		sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(vas));
		return std::string(s_str);
	}
	const VAStatus m_vas;
};

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}

inline void ThrowIfFailed(VAStatus va_status, const char* func)
{
	if (va_status != VA_STATUS_SUCCESS)
	{
		printf("%s:%s (%d) failed with VAStatus %x,exit\n", __func__, func, __LINE__, va_status);   \
			throw VAException(va_status);
	}
}

class CScreenCaptureDX12VaApi{

public:
	CScreenCaptureDX12VaApi(void);
	~CScreenCaptureDX12VaApi();
	int StartCaptureDX11New(void);

private:

	void InitAdapter(void);
	int  DX11DesktopDuplication(void);
	//int  DX11ConfigVideoProcessor(void);
	int  DX11CreateTextures(void);
	int  DX11ProcessScreenCapture(void);
	void DX11CloseDevice(void);
	//int  DX11ShareHandleD11D12(void);
	void DX11CleanDesktopResource(void);
	void writeD3D11TextureToFile(const ComPtr<ID3D11Texture2D>&tex, int width, int height);
	//int  DX11MapToD12Texture(void);
	int  DX11AquireD12Resource(void);

	int  DX12CreateSharedRenderTargets(void);


	void CreateVASurfaces(void);

	// VAAPI Functions
	int InitializeVAAPI(void);

	//AVCodec const* find_hw_encoder(AVCodecID const codec_id);

	static const UINT FrameCount = 2;	// D3D12 Objects
	/*
	// Declare pointers for the required parameters
	AVCodecContext* codecCtx = nullptr;
	AVFormatContext* formatCtx = nullptr;
	AVStream* stream = nullptr;
	AVFrame* frame = nullptr;
	AVBufferRef* hw_device_ctx = nullptr;
	*/
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<IDXGIAdapter1> m_adapter;

	ComPtr<ID3D11Device> d3d11Device;
	ComPtr<ID3D11DeviceContext> d3d11Context;
	D3D_FEATURE_LEVEL d3d11FeatureLevel;

	ComPtr<ID3D12Device> d3d12Device;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12Resource> D3D12SharedResource;
	ID3D12Resource* D3D12SharedResource2 = nullptr;
	ComPtr<ID3D12Resource> readbackResource;
	HANDLE D3D12SharedHandle = nullptr;

	HANDLE m_renderSharedHandles[2] = { nullptr, nullptr };

	HRESULT hr;

	ID3D11VideoDevice* videoDevice = nullptr;
	ID3D11VideoContext* videoContext = nullptr;
	ID3D11VideoProcessorEnumerator* vpEnumerator = nullptr;
	ID3D11VideoProcessor* videoProcessor = nullptr;

	D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
	D3D11_TEXTURE2D_DESC inputDesc = {};
	ComPtr<ID3D11Texture2D> inputTexture;
	D3D11_TEXTURE2D_DESC outputDesc = {};
	ComPtr<ID3D11Texture2D> outputTexture;
	ComPtr<IDXGIOutputDuplication> duplication;
	ComPtr<IDXGIResource> desktopResource;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_renderTargets2[FrameCount];

	VADisplay vaDisplay;
	VAConfigID configID;
	VAContextID contextID;
	VADisplay m_vaDisplay = { };
	VASurfaceID vaSurfaces[FrameCount] = { };
	VASurfaceID m_VASurfaceBGRA = 0;
	VAProcPipelineCaps m_ProcPipelineCaps = { };


	bool isVideoProcEnabled;
	bool isFfmpegEnabled;
	int m_width = 2560;
	int m_height = 1440;
};