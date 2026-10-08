#ifndef MEDIACOMPONENTS_H
#define MEDIACOMPONENTS_H

#include "media_raii.h"

class DemuxReader final
{
public:
    bool open(const char *locator);
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
    bool open(const AVCodecParameters *parameters, AVRational packetTimeBase);
    int send(const AVPacket *packet);
    int receive(AVFrame *frame);
    AVCodecContext *context() const;
    void close();

private:
    AvCodecContextPtr m_context;
};

class HardwareDecoderDevice final
{
public:
    bool initialize(AVHWDeviceType type);
    AVBufferRef *context() const;
    void reset();

private:
    AvBufferRefPtr m_context;
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
    void attachRenderer(SDL_Renderer *renderer);
    bool ensureTexture(Uint32 format, int width, int height);
    SDL_Texture *texture() const;
    void releaseTexture();

private:
    SDL_Renderer *m_renderer = nullptr;
    SdlTexturePtr m_texture;
};

class FilterChain final
{
public:
    bool create(const char *description, AVFilterContext *source, AVFilterContext *sink);
    AVFilterGraph *graph() const;
    void reset();

private:
    AvFilterGraphPtr m_graph;
};

#endif // MEDIACOMPONENTS_H
