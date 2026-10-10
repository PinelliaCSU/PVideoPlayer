#include "mediacomponents.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#define av_log_info(fmt, ...) av_log(NULL, AV_LOG_INFO, fmt, ##__VA_ARGS__)
#define av_log_error(fmt, ...) av_log(NULL, AV_LOG_ERROR, fmt, ##__VA_ARGS__)

bool DemuxReader::open(const char *locator)
{
    return open(locator, nullptr, nullptr);
}

bool DemuxReader::open(const char *locator, InterruptCallback callback, void *opaque)
{
    close();
    AVFormatContext *allocated = avformat_alloc_context();
    if (!allocated) {
        return false;
    }
    allocated->interrupt_callback.callback = callback;
    allocated->interrupt_callback.opaque = opaque;
    AVFormatContext *context = nullptr;
    context = allocated;
    if (avformat_open_input(&context, locator, nullptr, nullptr) < 0) {
        avformat_free_context(allocated);
        return false;
    }
    if (avformat_find_stream_info(context, nullptr) < 0) {
        avformat_close_input(&context);
        return false;
    }
    m_context.reset(context);
    return true;
}

int DemuxReader::read(AVPacket *packet)
{
    return m_context ? av_read_frame(m_context.get(), packet) : AVERROR(EINVAL);
}

int DemuxReader::seek(int64_t timestamp, AVRational timeBase)
{
    if (!m_context) {
        return AVERROR(EINVAL);
    }
    const int64_t target = av_rescale_q(timestamp, timeBase, AV_TIME_BASE_Q);
    return avformat_seek_file(m_context.get(), -1, INT64_MIN, target, INT64_MAX, 0);
}

int DemuxReader::bestStream(AVMediaType type) const
{
    return m_context ? av_find_best_stream(m_context.get(), type, -1, -1, nullptr, 0) : -1;
}

AVFormatContext *DemuxReader::context() const
{
    return m_context.get();
}

void DemuxReader::close()
{
    m_context.reset();
}

bool DecoderComponent::open(const AVCodecParameters *parameters, AVRational packetTimeBase)
{
    close();
    const AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
    if (!codec) {
        return false;
    }

    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context || avcodec_parameters_to_context(context, parameters) < 0) {
        avcodec_free_context(&context);
        return false;
    }
    context->pkt_timebase = packetTimeBase;
    if (avcodec_open2(context, codec, nullptr) < 0) {
        avcodec_free_context(&context);
        return false;
    }
    m_context.reset(context);
    return true;
}

void DecoderComponent::attach(AVCodecContext *context)
{
    close();
    m_attached_context = context;
}

void DecoderComponent::detach()
{
    m_attached_context = nullptr;
}

int DecoderComponent::send(const AVPacket *packet)
{
    AVCodecContext *context = m_attached_context ? m_attached_context : m_context.get();
    return context ? avcodec_send_packet(context, packet) : AVERROR(EINVAL);
}

int DecoderComponent::receive(AVFrame *frame)
{
    AVCodecContext *context = m_attached_context ? m_attached_context : m_context.get();
    return context ? avcodec_receive_frame(context, frame) : AVERROR(EINVAL);
}

AVCodecContext *DecoderComponent::context() const
{
    return m_context.get();
}

void DecoderComponent::close()
{
    flush();
    m_attached_context = nullptr;
    m_context.reset();
}

void DecoderComponent::flush()
{
    AVCodecContext *context = m_attached_context ? m_attached_context : m_context.get();
    if (context) {
        avcodec_flush_buffers(context);
    }
}

bool HardwareDecoderDevice::initializeForDecoder(const AVCodec *codec, AVHWDeviceType preferredType)
{
    reset();
    if (!codec) {
        return false;
    }

    enum AVPixelFormat selectedFormat = AV_PIX_FMT_NONE;
    enum AVHWDeviceType selectedType = AV_HWDEVICE_TYPE_NONE;

    // 只接受可以通过 hw_device_ctx 接口使用的配置，逐项匹配解码器声明的硬件能力
    for (int index = 0;; ++index) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, index);
        if (!config) {
            break;
        }
        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
            continue;
        }
        if (preferredType != AV_HWDEVICE_TYPE_NONE && config->device_type != preferredType) {
            continue;
        }
        selectedFormat = config->pix_fmt;
        selectedType = config->device_type;
        break;
    }

    if (selectedType == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }

    AVBufferRef *context = nullptr;
    if (av_hwdevice_ctx_create(&context, selectedType, nullptr, nullptr, 0) < 0) {
        av_log_info("hardware device %s is not available\n", av_hwdevice_get_type_name(selectedType));
        return false;
    }
    av_log_info("using hardware decoder device %s, pixel format %s\n",
                av_hwdevice_get_type_name(selectedType),
                av_get_pix_fmt_name(selectedFormat) ? av_get_pix_fmt_name(selectedFormat) : "unknown");
    m_context.reset(context);
    m_pixelFormat = selectedFormat;
    m_deviceType = selectedType;
    return true;
}

AVBufferRef *HardwareDecoderDevice::context() const
{
    return m_context.get();
}

enum AVPixelFormat HardwareDecoderDevice::pixelFormat() const
{
    return m_pixelFormat;
}

enum AVHWDeviceType HardwareDecoderDevice::deviceType() const
{
    return m_deviceType;
}

bool HardwareDecoderDevice::isActive() const
{
    return static_cast<bool>(m_context) && m_pixelFormat != AV_PIX_FMT_NONE;
}

bool HardwareDecoderDevice::toSoftwareFrame(const AVFrame *hardwareFrame, AVFrame *softwareFrame) const
{
    if (!isActive() || !hardwareFrame || !softwareFrame) {
        return false;
    }
    if (hardwareFrame->format != m_pixelFormat) {
        return false;
    }
    av_frame_unref(softwareFrame);
    return av_hwframe_transfer_data(softwareFrame, hardwareFrame, 0) >= 0;
}

void HardwareDecoderDevice::reset()
{
    m_pixelFormat = AV_PIX_FMT_NONE;
    m_deviceType = AV_HWDEVICE_TYPE_NONE;
    m_context.reset();
}

bool AudioOutputDevice::open(const SDL_AudioSpec &wanted, SDL_AudioSpec *obtained)
{
    close();
    SDL_AudioSpec desired = wanted;
    m_open = SDL_OpenAudio(&desired, obtained) == 0;
    return m_open;
}

void AudioOutputDevice::pause(bool paused)
{
    if (m_open) {
        SDL_PauseAudio(paused ? 1 : 0);
    }
}

void AudioOutputDevice::close()
{
    if (m_open) {
        SDL_CloseAudio();
        m_open = false;
    }
}

bool AudioOutputDevice::isOpen() const
{
    return m_open;
}

bool VideoOutputResources::ensureWindow(void *nativeHandle, int *width, int *height)
{
    if (!m_window) {
        m_window.reset(SDL_CreateWindowFrom(nativeHandle));
        if (!m_window) {
            av_log_error("SDL_CreateWindowFrom failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_GetWindowSize(m_window.get(), width, height);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    } else {
        SDL_SetWindowSize(m_window.get(), *width, *height);
    }
    return true;
}

bool VideoOutputResources::setFullScreen(bool fullScreen)
{
    if (!m_window) {
        return false;
    }
    SDL_SetWindowFullscreen(m_window.get(), fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    return true;
}

void VideoOutputResources::getWindowSize(int *width, int *height) const
{
    if (!m_window) {
        return;
    }
    SDL_GetWindowSize(m_window.get(), width, height);
}

bool VideoOutputResources::ensureRenderer()
{
    if (!m_window) {
        return false;
    }

    /*
     * 当调用 SDL_AudioClose 后，渲染器可能会失效【这就很扯，SDL_AudioClose 为什么要影响渲染器呢】
     * mark: 复用窗口时，检测旧渲染器是否可用
     * 用 SDL_RenderClear 做真实检测（探针 CreateTexture 不可靠，D3D 设备丢失只有真正渲染时才暴露）
     */
    if (m_renderer) {
        SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 255);
        if (SDL_RenderClear(m_renderer.get()) < 0) {
            av_log_error("SDL_RenderClear error: %s\n", SDL_GetError());
            destroyRenderer();
        }
    }

    if (!m_renderer) {
        m_renderer.reset(SDL_CreateRenderer(m_window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
        // 如果创建失败，那么就创建一个普通的渲染器
        if (!m_renderer) {
            m_renderer.reset(SDL_CreateRenderer(m_window.get(), -1, 0));
        }
    }

    if (m_renderer) {
        SDL_RendererInfo info;
        // 可以发现会从 direct3d 变成了 opengl
        if (!SDL_GetRendererInfo(m_renderer.get(), &info))
            av_log_info("Initialized %s renderer.\n", info.name);
    }
    return static_cast<bool>(m_renderer);
}

bool VideoOutputResources::recreateRenderer()
{
    // 渲染器一定会失效（例如音频设备关闭），所以直接销毁重建
    destroyRenderer();
    return ensureRenderer();
}

void VideoOutputResources::destroyRenderer()
{
    // 纹理依赖渲染器，必须先释放纹理
    releaseTexture();
    m_renderer.reset();
}

bool VideoOutputResources::isReady() const
{
    return static_cast<bool>(m_window) && static_cast<bool>(m_renderer);
}

SDL_Window *VideoOutputResources::window() const
{
    return m_window.get();
}

SDL_Renderer *VideoOutputResources::renderer() const
{
    return m_renderer.get();
}

bool VideoOutputResources::ensureTexture(Uint32 format, int width, int height, SDL_BlendMode blendMode)
{
    if (!m_renderer) {
        return false;
    }
    int currentWidth = 0;
    int currentHeight = 0;
    Uint32 currentFormat = 0;
    int access = 0;
    const bool needsReplacement = !m_texture
        || SDL_QueryTexture(m_texture.get(), &currentFormat, &access, &currentWidth, &currentHeight) < 0
        || currentFormat != format || currentWidth != width || currentHeight != height;
    if (needsReplacement) {
        m_texture.reset(SDL_CreateTexture(m_renderer.get(), format, SDL_TEXTUREACCESS_STREAMING, width, height));
        if (!m_texture) {
            av_log_error("SDL_CreateTexture failed: %s\n", SDL_GetError());
            return false;
        }
    }
    return SDL_SetTextureBlendMode(m_texture.get(), blendMode) == 0;
}

SDL_Texture *VideoOutputResources::texture() const
{
    return m_texture.get();
}

void VideoOutputResources::releaseTexture()
{
    m_texture.reset();
}

void VideoOutputResources::beginFrame()
{
    if (!m_renderer) {
        return;
    }
    SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 255);
    SDL_RenderClear(m_renderer.get());
}

void VideoOutputResources::presentTexture(const SDL_Rect &rect, bool flipVertical)
{
    if (!m_renderer || !m_texture) {
        return;
    }
    SDL_RenderCopyEx(m_renderer.get(), m_texture.get(), nullptr, &rect, 0, nullptr,
                     flipVertical ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE);
}

void VideoOutputResources::endFrame()
{
    if (m_renderer) {
        SDL_RenderPresent(m_renderer.get());
    }
}

void VideoOutputResources::reset()
{
    releaseTexture();
    m_renderer.reset();
    m_window.reset();
}

bool FilterChain::create(const char *description, const AVFrame *inputTemplate, AVRational inputTimeBase)
{
    reset();
    if (!description || !*description || !inputTemplate) {
        return false;
    }

    m_graph.reset(avfilter_graph_alloc());
    if (!m_graph) {
        return false;
    }

    const AVFilter *bufferSource = avfilter_get_by_name("buffer");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");
    if (!bufferSource || !bufferSink) {
        reset();
        return false;
    }

    char arguments[512];
    snprintf(arguments, sizeof(arguments),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             inputTemplate->width, inputTemplate->height, inputTemplate->format,
             inputTimeBase.num, inputTimeBase.den,
             inputTemplate->sample_aspect_ratio.num, inputTemplate->sample_aspect_ratio.den);

    if (avfilter_graph_create_filter(&m_source, bufferSource, "in", arguments, nullptr, m_graph.get()) < 0) {
        av_log_error("cannot create buffer source for filter chain\n");
        reset();
        return false;
    }
    if (avfilter_graph_create_filter(&m_sink, bufferSink, "out", nullptr, nullptr, m_graph.get()) < 0) {
        av_log_error("cannot create buffer sink for filter chain\n");
        reset();
        return false;
    }

    /*
     * avfilter_graph_parse_ptr 会接管并可能替换输入/输出端点列表，
     * 因此用持有者对象在任意返回路径上统一释放。
     */
    struct InOutHolder
    {
        AVFilterInOut *inputs = avfilter_inout_alloc();
        AVFilterInOut *outputs = avfilter_inout_alloc();
        ~InOutHolder()
        {
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
        }
    } endpoints;

    if (!endpoints.inputs || !endpoints.outputs) {
        reset();
        return false;
    }
    endpoints.inputs->name = av_strdup("out");
    endpoints.inputs->filter_ctx = m_sink;
    endpoints.inputs->pad_idx = 0;
    endpoints.inputs->next = nullptr;
    endpoints.outputs->name = av_strdup("in");
    endpoints.outputs->filter_ctx = m_source;
    endpoints.outputs->pad_idx = 0;
    endpoints.outputs->next = nullptr;

    if (avfilter_graph_parse_ptr(m_graph.get(), description,
                                 &endpoints.inputs, &endpoints.outputs, nullptr) < 0) {
        av_log_error("cannot parse filter description: %s\n", description);
        reset();
        return false;
    }
    if (avfilter_graph_config(m_graph.get(), nullptr) < 0) {
        av_log_error("cannot configure filter graph\n");
        reset();
        return false;
    }
    av_log_info("filter chain created: %s\n", description);
    return true;
}

bool FilterChain::isActive() const
{
    return m_graph && m_source && m_sink;
}

int FilterChain::push(const AVFrame *frame)
{
    if (!m_source) {
        return AVERROR(EINVAL);
    }
    return av_buffersrc_add_frame_flags(m_source, const_cast<AVFrame *>(frame), AV_BUFFERSRC_FLAG_KEEP_REF);
}

int FilterChain::pull(AVFrame *frame)
{
    if (!m_sink) {
        return AVERROR(EINVAL);
    }
    return av_buffersink_get_frame(m_sink, frame);
}

AVRational FilterChain::outputTimeBase() const
{
    if (!m_sink) {
        return AVRational{0, 1};
    }
    return av_buffersink_get_time_base(m_sink);
}

void FilterChain::reset()
{
    m_source = nullptr;
    m_sink = nullptr;
    m_graph.reset();
}
