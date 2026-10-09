#ifndef MEDIA_RAII_H
#define MEDIA_RAII_H

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <SDL.h>
}

struct AvFormatContextDeleter
{
    void operator()(AVFormatContext *context) const
    {
        avformat_close_input(&context);
    }
};

struct AvCodecContextDeleter
{
    void operator()(AVCodecContext *context) const
    {
        avcodec_free_context(&context);
    }
};

struct AvFrameDeleter
{
    void operator()(AVFrame *frame) const
    {
        av_frame_free(&frame);
    }
};

struct AvBufferRefDeleter
{
    void operator()(AVBufferRef *buffer) const
    {
        av_buffer_unref(&buffer);
    }
};

struct AvPacketDeleter
{
    void operator()(AVPacket *packet) const
    {
        av_packet_free(&packet);
    }
};

struct SdlTextureDeleter
{
    void operator()(SDL_Texture *texture) const
    {
        SDL_DestroyTexture(texture);
    }
};

struct SdlRendererDeleter
{
    void operator()(SDL_Renderer *renderer) const
    {
        SDL_DestroyRenderer(renderer);
    }
};

struct SdlWindowDeleter
{
    void operator()(SDL_Window *window) const
    {
        SDL_DestroyWindow(window);
    }
};

struct SdlMutexDeleter
{
    void operator()(SDL_mutex *mutex) const
    {
        SDL_DestroyMutex(mutex);
    }
};

struct SdlCondDeleter
{
    void operator()(SDL_cond *condition) const
    {
        SDL_DestroyCond(condition);
    }
};

struct SwsContextDeleter
{
    void operator()(SwsContext *context) const
    {
        sws_freeContext(context);
    }
};

struct SwrContextDeleter
{
    void operator()(SwrContext *context) const
    {
        swr_free(&context);
    }
};

struct AvFilterGraphDeleter
{
    void operator()(AVFilterGraph *graph) const
    {
        avfilter_graph_free(&graph);
    }
};

struct AvFilterInOutDeleter
{
    void operator()(AVFilterInOut *inOut) const
    {
        avfilter_inout_free(&inOut);
    }
};

using AvFormatContextPtr = std::unique_ptr<AVFormatContext, AvFormatContextDeleter>;
using AvCodecContextPtr = std::unique_ptr<AVCodecContext, AvCodecContextDeleter>;
using AvFramePtr = std::unique_ptr<AVFrame, AvFrameDeleter>;
using AvBufferRefPtr = std::unique_ptr<AVBufferRef, AvBufferRefDeleter>;
using AvPacketPtr = std::unique_ptr<AVPacket, AvPacketDeleter>;
using SdlTexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;
using SdlRendererPtr = std::unique_ptr<SDL_Renderer, SdlRendererDeleter>;
using SdlWindowPtr = std::unique_ptr<SDL_Window, SdlWindowDeleter>;
using SdlMutexPtr = std::unique_ptr<SDL_mutex, SdlMutexDeleter>;
using SdlCondPtr = std::unique_ptr<SDL_cond, SdlCondDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using AvFilterGraphPtr = std::unique_ptr<AVFilterGraph, AvFilterGraphDeleter>;
using AvFilterInOutPtr = std::unique_ptr<AVFilterInOut, AvFilterInOutDeleter>;

#endif // MEDIA_RAII_H
