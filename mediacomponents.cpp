#include "mediacomponents.h"

extern "C" {
#include <libavutil/avutil.h>
}

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

bool HardwareDecoderDevice::initialize(AVHWDeviceType type)
{
    reset();
    AVBufferRef *context = nullptr;
    if (av_hwdevice_ctx_create(&context, type, nullptr, nullptr, 0) < 0) {
        return false;
    }
    m_context.reset(context);
    return true;
}

AVBufferRef *HardwareDecoderDevice::context() const
{
    return m_context.get();
}

void HardwareDecoderDevice::reset()
{
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

void VideoOutputResources::attachRenderer(SDL_Renderer *renderer)
{
    if (m_renderer != renderer) {
        releaseTexture();
        m_renderer = renderer;
    }
}

bool VideoOutputResources::ensureTexture(Uint32 format, int width, int height)
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
        m_texture.reset(SDL_CreateTexture(m_renderer, format, SDL_TEXTUREACCESS_STREAMING, width, height));
    }
    return static_cast<bool>(m_texture);
}

SDL_Texture *VideoOutputResources::texture() const
{
    return m_texture.get();
}

void VideoOutputResources::releaseTexture()
{
    m_texture.reset();
}

bool FilterChain::create(const char *description, AVFilterContext *source, AVFilterContext *sink)
{
    reset();
    m_graph.reset(avfilter_graph_alloc());
    if (!m_graph) {
        return false;
    }
    AVFilterInOut *inputs = nullptr;
    AVFilterInOut *outputs = nullptr;
    const int result = avfilter_graph_parse_ptr(m_graph.get(), description, &inputs, &outputs, nullptr);
    AvFilterInOutPtr inputOwner(inputs);
    AvFilterInOutPtr outputOwner(outputs);
    if (result < 0) {
        return false;
    }
    if (source && sink) {
        if (avfilter_link(source, 0, sink, 0) < 0) {
            return false;
        }
    }
    return avfilter_graph_config(m_graph.get(), nullptr) >= 0;
}

AVFilterGraph *FilterChain::graph() const
{
    return m_graph.get();
}

void FilterChain::reset()
{
    m_graph.reset();
}
