#ifndef MEDIACOMPONENTS_H
#define MEDIACOMPONENTS_H

#include "media_raii.h"

class DemuxReader final
{
public:
    using InterruptCallback = int (*)(void *);

    bool open(const char *locator);
    bool open(const char *locator, InterruptCallback callback, void *opaque);
    int read(AVPacket *packet);
    int seek(int64_t timestamp, AVRational timeBase);
    int bestStream(AVMediaType type) const;
    AVFormatContext *context() const;
    void close();

private:
    AvFormatContextPtr m_context;
};

class DecoderComponent final
{
public:
    void attach(AVCodecContext *context);
    void detach();
    bool open(const AVCodecParameters *parameters, AVRational packetTimeBase);
    int send(const AVPacket *packet);
    int receive(AVFrame *frame);
    void flush();
    AVCodecContext *context() const;
    void close();

private:
    AVCodecContext *m_attached_context = nullptr;
    AvCodecContextPtr m_context;
};

class HardwareDecoderDevice final
{
public:
    // 依据解码器声明的 hw_configs 选择可用的硬件像素格式并创建设备上下文。
    // preferredType 为 AV_HWDEVICE_TYPE_NONE 时接受解码器支持的第一种设备类型。
    bool initializeForDecoder(const AVCodec *codec, AVHWDeviceType preferredType);
    AVBufferRef *context() const;
    enum AVPixelFormat pixelFormat() const;
    enum AVHWDeviceType deviceType() const;
    bool isActive() const;
    // 把硬件解码帧转换为软件帧，使后续渲染/滤镜逻辑无需感知硬件解码
    bool toSoftwareFrame(const AVFrame *hardwareFrame, AVFrame *softwareFrame) const;
    void reset();

private:
    AvBufferRefPtr m_context;
    enum AVPixelFormat m_pixelFormat = AV_PIX_FMT_NONE;
    enum AVHWDeviceType m_deviceType = AV_HWDEVICE_TYPE_NONE;
};

class AudioOutputDevice final
{
public:
    bool open(const SDL_AudioSpec &wanted, SDL_AudioSpec *obtained);
    void pause(bool paused);
    void close();
    bool isOpen() const;

private:
    bool m_open = false;
};

class VideoOutputResources final
{
public:
    // 窗口：从原生窗口句柄创建（若尚未创建），并把当前窗口尺寸写回 width/height
    bool ensureWindow(void *nativeHandle, int *width, int *height);
    bool setFullScreen(bool fullScreen);
    void getWindowSize(int *width, int *height) const;

    // 渲染器：复用前先用真实清屏探测有效性，失效或缺失时重建
    bool ensureRenderer();
    bool recreateRenderer();
    void destroyRenderer();

    bool isReady() const;
    SDL_Window *window() const;
    SDL_Renderer *renderer() const;

    // 纹理：尺寸或像素格式变化时重建，并统一设置混合模式
    bool ensureTexture(Uint32 format, int width, int height, SDL_BlendMode blendMode);
    SDL_Texture *texture() const;
    void releaseTexture();

    // 渲染循环原语
    void beginFrame();
    void presentTexture(const SDL_Rect &rect, bool flipVertical);
    void endFrame();

    void reset();

private:
    SdlWindowPtr m_window;
    SdlRendererPtr m_renderer;
    SdlTexturePtr m_texture;
};

class FilterChain final
{
public:
    // 依据输入帧格式构建 buffer -> description -> buffersink 的视频滤镜图；
    // description 为空时返回 false（表示不启用滤镜）。
    bool create(const char *description, const AVFrame *inputTemplate, AVRational inputTimeBase);
    bool isActive() const;

    // 送入一帧并取回处理后的帧：push 返回 0 表示入队成功，
    // pull 返回 >= 0 表示取到一帧，返回 AVERROR(EAGAIN) 表示当前没有可输出的帧。
    int push(const AVFrame *frame);
    int pull(AVFrame *frame);
    AVRational outputTimeBase() const;

    void reset();

private:
    AvFilterGraphPtr m_graph;
    AVFilterContext *m_source = nullptr;
    AVFilterContext *m_sink = nullptr;
};

#endif // MEDIACOMPONENTS_H
