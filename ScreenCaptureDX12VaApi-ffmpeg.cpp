#include <iostream>

#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libavutil/hwcontext.h>
}

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <chrono>
#include <thread>
#include <vfw.h>
#include "ScreenCaptureDX12VaApi.h"
#include <vector>

#define USE_HW_ACC_FFMPEG 1

//using namespace System;
using namespace Microsoft::WRL;
#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

uint8_t frameData[2560 * 1440 * 4];
const char filename[128] = "output.mp4";
AVCodecID codecId = AV_CODEC_ID_H264;  // AV_CODEC_ID_MPEG2VIDEO
const int encFps = 30;
const int64_t encBitrate = 400000;
AVPixelFormat srcPixelFormat = AV_PIX_FMT_YUV420P;
AVPixelFormat encPixelFormat = AV_PIX_FMT_NV12;// AV_PIX_FMT_YUV420P;


void CScreenCaptureDX12VaApi::generateScreenNV12(uint8_t* buffer, int width, int height) {
    int lumaSize = width * height;               // Size of the Y (luma) plane
    int chromaSize = (width / 2) * (height / 2); // Size of the UV chroma plane (interleaved)
    static int c = 0;

    // Set Luma plane (Y)
    memset(buffer, 76, lumaSize); // 76 for red brightness

    // Set Chroma plane (UV, interleaved)
    uint8_t* uvPlane = buffer + lumaSize;
    for (int i = 0; i < chromaSize; i++) {
        uvPlane[i * 2] = 85;   // U (blue chroma)
        uvPlane[i * 2 + 1] = (c & 255); // V (red chroma)
    }
    c += 2;
}

void listHardwareConfigs(const AVCodec* codec) {
    //if (!codec->hw_configs) {
    //    std::cout << "No hardware configurations available for this codec.\n";
    //    return;
    //}

    std::cout << "Available hardware configurations for codec: " << codec->name << "\n";

    // Loop through the hardware configurations
    for (int i = 0;; i++) {
        const AVCodecHWConfig* hwConfig = avcodec_get_hw_config(codec, i);
        if (!hwConfig) {
            break; // No more configurations
        }

        // Print configuration details
        std::cout << "HW Config " << i << ": "
            << "Device Type: " << av_hwdevice_get_type_name(hwConfig->device_type) << "\n";

        // Check if it supports both decoding and encoding
        if (hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
            std::cout << "  - Supports AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX\n";
        }

        if (hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX) {
            std::cout << "  - Supports AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX\n";
        }

        if (hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL) {
            std::cout << "  - Supports AV_CODEC_HW_CONFIG_METHOD_INTERNAL\n";
        }
    }
}

AVCodec const* CScreenCaptureDX12VaApi::find_hw_encoder(AVCodecID const codec_id)
{
    AVCodec const* any_codec = nullptr;

    AVCodec const* cur_codec;
    void* iter = 0;
    while ((cur_codec = av_codec_iterate(&iter)))
    {
        //std::cout << cur_codec->id << "  " << cur_codec->name << "  " << cur_codec->long_name << std::endl;
        //if (cur_codec->id == codec_id)
        //    std::cout << "================= " << cur_codec->id << "  " << cur_codec->name << "  " << cur_codec->long_name << " isEnc: " << av_codec_is_encoder(cur_codec) << std::endl;
        if (strcmp(cur_codec->name, "h264") > 0)
            std::cout << "================= " << cur_codec->id << "  " << cur_codec->name << "  " << cur_codec->long_name << " isEnc: " << av_codec_is_encoder(cur_codec) << std::endl;
        if (!av_codec_is_encoder(cur_codec))
            continue;
        if (cur_codec->id != codec_id)
            continue;

        if (!any_codec)
            any_codec = cur_codec;

        //currently av1 codec on apple IS NOT implemented via videotoolbox, though it presents in the list, just it fails afterwards.
        if (cur_codec->capabilities & (AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_HYBRID))
            return cur_codec;

        if (codec_id == AV_CODEC_ID_AV1 && (!strcmp(cur_codec->name, "av1") || !strcmp(cur_codec->name, "av1_videotoolbox")))
            return cur_codec;
    }

    return any_codec;
}


// Initialize FFmpeg
void CScreenCaptureDX12VaApi::initializeEncoder(AVCodecContext** codecCtx, AVFormatContext** formatCtx, AVStream** stream, AVFrame** frame, int width, int height)
{
    int ret;

    av_log_set_level(AV_LOG_DEBUG);


    const char* codecName = "h264_d3d11va";
    const AVCodec* codec2 = avcodec_find_encoder_by_name(codecName);
    if (!codec2) {
        std::cerr << "Codec " << codecName << " not found.\n";
        return;
    }

    // List hardware configurations
    //listHardwareConfigs(codec2);

#ifdef USE_HW_ACC_FFMPEG
    codecId = AV_CODEC_ID_H264;  // AV_CODEC_ID_MPEG2VIDEO
    //const AVCodec* codec = find_hw_encoder(codecId);
    //const AVCodec* codec = avcodec_find_encoder(codecId);
    const AVCodec* codec = avcodec_find_decoder_by_name("h264");
    const AVCodecHWConfig* config = nullptr;
    bool codecSupported = false;
    for (int i = 0; (config = avcodec_get_hw_config(codec, i)); i++) {
        if (config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
            std::cout << "D3D11VA is supported!" << std::endl;
            codecSupported = true;
            break;
        }
    }
    if (!codecSupported)
    {
        std::cerr << "D3D11VA is not supported with codec Id: " << codecId << std::endl;
        //return;
    }

#else
    const AVCodec* codec = avcodec_find_encoder(codecId);
#endif
    if (!codec) {

        std::cerr << "Codec not found" << std::endl;
        return;
    }
    std::cout << "codec found" << std::endl;

#ifdef USE_HW_ACC_FFMPEG
    encPixelFormat = AV_PIX_FMT_D3D11; // Using D3D11 pixel format
    const std::string hw_device_name = "d3d11va";
    AVHWDeviceType device_type = av_hwdevice_find_type_by_name(hw_device_name.c_str());

    hw_device_ctx = nullptr;
    ret = av_hwdevice_ctx_create(&hw_device_ctx, device_type, nullptr, nullptr, 0);
    if (ret < 0) {
        std::cerr << "Failed to create D3D11VA device context: " << ret << std::endl;
        return;
    }
#endif

    avformat_alloc_output_context2(formatCtx, nullptr, "mp4", filename);
    *stream = avformat_new_stream(*formatCtx, codec);
    (*stream)->time_base = av_d2q(1.0 / encFps, 120);
    (*stream)->id = (int)((*formatCtx)->nb_streams - 1);

    *codecCtx = avcodec_alloc_context3(codec);
    (*codecCtx)->pix_fmt = encPixelFormat;
    (*codecCtx)->width = width;
    (*codecCtx)->height = height;
    (*codecCtx)->time_base.num = 1;
    (*codecCtx)->time_base.den = encFps;
    (*codecCtx)->bit_rate = encBitrate;
    (*codecCtx)->gop_size = 12;
    (*codecCtx)->max_b_frames = 2;
    (*codecCtx)->framerate.num = encFps;
    (*codecCtx)->framerate.den = 1;
    (*codecCtx)->codec_id = codecId;
    (*codecCtx)->codec_type = AVMEDIA_TYPE_VIDEO;
#ifdef USE_HW_ACC_FFMPEG
    (*codecCtx)->hw_device_ctx = av_buffer_ref(hw_device_ctx); // Attach the hardware device context
    if (av_hwdevice_ctx_init((*codecCtx)->hw_device_ctx) < 0) {
        return;
    }
#endif
    //(*codecCtx)->profile = FF_PROFILE_H264_MAIN;
    //(*codecCtx)->level = FF_PROFILE_H264_MAIN;
    //av_opt_set((*codecCtx)->priv_data, "profile", "high", 0);
    //av_opt_set((*codecCtx)->priv_data, "level", "4.1", 0);

    if ((*formatCtx)->oformat->flags & AVFMT_GLOBALHEADER)
        (*codecCtx)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    (*formatCtx)->oformat = av_guess_format("mp4", nullptr, nullptr);

    ret = avcodec_open2(*codecCtx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return;
    }
    std::cout << "Codec codec opened" << std::endl;

    (*frame) = av_frame_alloc();
    (*frame)->format = (*codecCtx)->pix_fmt;
    (*frame)->width = (*codecCtx)->width;
    (*frame)->height = (*codecCtx)->height;
    if (av_frame_get_buffer((*frame), 32) < 0) {
        std::cerr << "Failed to allocate frame buffer" << std::endl;
        av_frame_free(&(*frame));
        return;
    }

    ret = avcodec_parameters_from_context((*stream)->codecpar, (*codecCtx));
    if (ret < 0) {
        std::cerr << "Failed to copy codec parameters from codec context" << std::endl;
        return;
    }
    av_dump_format(*formatCtx, 1, filename, 1);

    ret = avio_open(&(*formatCtx)->pb, filename, AVIO_FLAG_WRITE);
    if (ret != 0)
    {
        std::cerr << "could not open " << filename << std::endl;
        return;
    }

    ret = avformat_write_header(*formatCtx, nullptr);
    if (ret < 0) {
        std::cerr << "Error writing header" << std::endl;
    }
}


void CScreenCaptureDX12VaApi::FfmpegEncodeFrame(uint8_t* frameData, AVCodecContext* codecContext, AVFormatContext* formatContext, AVStream* stream, AVFrame* frame)
{
    static int frameIndex = 0;

    int ret = av_frame_make_writable(frame);
    if (ret < 0)
    {
        std::cerr << "frame not writable" << std::endl;
        return;
    }

    printf("frameIndex: %d Frame format: %d, width: %d, height: %d\n", frameIndex, frame->format, frame->width, frame->height);

    //int copysize = codecContext->width * codecContext->height * 3 / 2;  // AV_PIX_FMT_YUV420P
    // Copy pixel data to the frame, avoid copy by using directly GPU memory
    //memcpy(frame->data[0], frameData, static_cast<size_t>(copysize));

    AVPacket packet = { 0 };

    // Encode the frame
    frame->pts = frameIndex++;

    ret = avcodec_send_frame(codecContext, frame);
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext, &packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        av_packet_rescale_ts(&packet, codecContext->time_base, stream->time_base);
        packet.stream_index = stream->index;
        av_interleaved_write_frame(formatContext, &packet);
        av_packet_unref(&packet);
    }
}

void CScreenCaptureDX12VaApi::CloseFfmpeg(void)
{
    if (codecCtx)
    {
        avcodec_free_context(&codecCtx);
    }
    if (formatCtx)
    {

        av_write_trailer(formatCtx); // Finalize the format
        avformat_free_context(formatCtx); // Free stream, format context
        if (formatCtx->pb)
        {
            avio_close(formatCtx->pb);
        }
    }
    if (frame)
    {
        av_frame_free(&frame);
    }
#ifdef USE_HW_ACC_FFMPEG
    av_buffer_unref(&hw_device_ctx);
#endif
}
