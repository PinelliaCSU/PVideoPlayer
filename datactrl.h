#ifndef DATACTRL_H
#define DATACTRL_H

#define __STDC_CONSTANT_MACROS
#include <new>
#include <string>
#include <thread>
#include "media_raii.h"
#include "mediacomponents.h"

extern "C"{
#include <libavcodec/avcodec.h>
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
// #include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include "SDL.h"
#include"libavutil/frame.h"
}


#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* Step size for volume control in dB */
#define SDL_VOLUME_STEP (0.75)

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define USE_ONEPASS_SUBTITLE_RENDER 1



//数据包列表
typedef struct MyAVPacketList {
    AVPacket pkt{};
    struct MyAVPacketList *next = nullptr;
    int serial = 0;

    // 节点拥有自己的 AVPacket 引用，析构时统一 unref，避免各条释放路径手工遗漏
    ~MyAVPacketList()
    {
        av_packet_unref(&pkt);
    }
} MyAVPacketList;

//数据包队列
typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
    SdlMutexPtr owned_mutex;
    SdlCondPtr owned_cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

//音频参数
typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;

//时钟
typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

/* Common struct for handling all types of decoded data and allocated render buffers. */
//解码后的帧
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
} Frame;

//帧队列
typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex *mutex;
    SDL_cond *cond;
    PacketQueue *pktq;
    SdlMutexPtr owned_mutex;
    SdlCondPtr owned_cond;
} FrameQueue;

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

//解码器，管理数据队列
typedef struct Decoder {
    AVPacket pkt;
    PacketQueue *queue;
    AVCodecContext *avctx;
    AvCodecContextPtr owned_avctx;
    DecoderComponent component;
    int pkt_serial;
    int finished;
    int packet_pending;
    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    std::thread decode_thread;

    // 解码器拥有待处理包（packet_pending 时由 send 失败暂存），析构时统一 unref
    ~Decoder()
    {
        av_packet_unref(&pkt);
    }
} Decoder;

//视频状态，管理所有的视频信息及数据
typedef struct VideoState {
    std::thread read_tid; //读取线程
    AVInputFormat *iformat;
    int abort_request; //停止读取标志
    int force_refresh;
    int paused; //暂停播放标志，true代表现在处于暂停状态
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    AvFormatContextPtr owned_ic;
    DemuxReader demux_reader;
    HardwareDecoderDevice hw_device; // 视频硬件解码设备（未启用时为 inactive）
    FilterChain video_filter;        // 可选视频滤镜链（描述为空时不启用）
    std::string filter_description;
    int realtime;

    Clock audclk;
    Clock vidclk;
    Clock extclk;

    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;

    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    int audio_volume;

    struct AudioParams audio_src;

    struct AudioParams audio_tgt;
    SwrContextPtr owned_swr_ctx;
    int frame_drops_early;
    int frame_drops_late;

    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    // RDFTContext *rdft;
    int rdft_bits;
    // FFTSample *rdft_data;
    int xpos;
    double last_vis_time;

    SDL_Texture *sub_texture;
    SDL_Texture *vid_texture;

    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;

    double frame_timer;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    SwsContextPtr owned_img_convert_ctx;
    SwsContextPtr owned_sub_convert_ctx;
    int eof;

    std::string filename;
    int width, height, xleft, ytop;
    int step;

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;
    SdlCondPtr owned_continue_read_thread;
} VideoState;






static AVPacket flush_pkt;

//数据包队列存放数据包（供队列内部使用）
static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;

    if (q->abort_request)
        return -1;

    /*
     * 节点接管 pkt 的所有权（浅拷贝转移引用），由 MyAVPacketList 析构统一 unref。
     * 注意不能对 flush_pkt 使用 av_packet_move_ref：它会被重置，导致后续
     * pkt.data == flush_pkt.data 的刷新包判定失效。
     */
    pkt1 = new (std::nothrow) MyAVPacketList();
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if (pkt == &flush_pkt)
        q->serial++;
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;
    /* XXX: should duplicate packet data in DV case */
    SDL_CondBroadcast(q->cond);
    return 0;
}

//数据包队列存放数据包
static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt);
    SDL_UnlockMutex(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_packet_unref(pkt);

    return ret;
}

//数据包队列存放空数据包
static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

/* packet queue handling */
//数据包队列初始化
static int packet_queue_init(PacketQueue *q)
{
    q->first_pkt = nullptr;
    q->last_pkt = nullptr;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    q->serial = 0;
    q->owned_mutex.reset(SDL_CreateMutex());
    q->mutex = q->owned_mutex.get();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->owned_cond.reset(SDL_CreateCond());
    q->cond = q->owned_cond.get();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        q->owned_mutex.reset();
        q->mutex = nullptr;
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;
    return 0;
}
//数据包队列清空
static void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        delete pkt; // 析构函数负责 unref 节点持有的 AVPacket
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    SDL_UnlockMutex(q->mutex);
}
//数据包队列销毁
static void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);
    q->owned_cond.reset();
    q->owned_mutex.reset();
    q->cond = nullptr;
    q->mutex = nullptr;
}
//数据包队列停用
static void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}
//数据包队列开始使用
static void packet_queue_start(PacketQueue *q)
{
    //初始化清理包
    av_init_packet(&flush_pkt);
    flush_pkt.data = (uint8_t *)&flush_pkt;

    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt);
    SDL_UnlockMutex(q->mutex);
}

static int packet_queue_packet_count(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);
    const int count = q->nb_packets;
    SDL_UnlockMutex(q->mutex);
    return count;
}

static int packet_queue_serial(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);
    const int serial = q->serial;
    SDL_UnlockMutex(q->mutex);
    return serial;
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
//从数据包队列中获取数据包
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            q->duration -= pkt1->pkt.duration;
            // 引用转移给调用方，节点析构时不再重复释放
            av_packet_move_ref(pkt, &pkt1->pkt);
            if (serial)
                *serial = pkt1->serial;
            delete pkt1;
            ret = 1;
            break;
        }
        else if (!block) {
            ret = 0;
            break;
        }
        else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}






//解码器初始化（绑定解码结构体、数据包队列、信号量，初始化pts）
static void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
    av_packet_unref(&d->pkt);
    d->pkt = AVPacket{};
    d->pkt_serial = 0;
    d->finished = 0;
    d->packet_pending = 0;
    d->avctx = avctx;
    d->component.attach(avctx);
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
}


static int decoder_reorder_pts = -1;
// for循环的第一步是读取帧，最后一步是把包写入解码器，看久一点才会知道数据是哪里写入哪里读取的。
static int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    int ret = AVERROR(EAGAIN);

    for (;;) {
        AVPacket pkt;
        // 1. 流连续情况下获取解码后的帧
        if (packet_queue_serial(d->queue) == d->pkt_serial) { // 1.1 先判断是否是同一播放序列的数据
            do {
                if (d->queue->abort_request)
                    return -1;  // 是否请求退出
                // 1.2. 获取解码帧
                switch (d->avctx->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    ret = d->component.receive(frame);
                    //printf("frame pts:%ld, dts:%ld\n", frame->pts, frame->pkt_dts);
                    if (ret >= 0) {
                        if (decoder_reorder_pts == -1) {
                            frame->pts = frame->best_effort_timestamp;
                        } else if (!decoder_reorder_pts) {
                            frame->pts = frame->pkt_dts;
                        }
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    ret = d->component.receive(frame);
                    if (ret >= 0) {
                        AVRational tb = {1, frame->sample_rate};    //
                        if (frame->pts != AV_NOPTS_VALUE) {
                            // 如果frame->pts正常则先将其从pkt_timebase转成{1, frame->sample_rate}
                            // pkt_timebase实质就是stream->time_base
                            // printf("decoder_decode_frame-bfroe frame->pts=%lld, pkt_timebase=%d/%d, tb=%d/%d\n", frame->pts, d->avctx->pkt_timebase.num, d->avctx->pkt_timebase.den, tb.num, tb.den);
                            frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
                            // printf("decoder_decode_frame frame->pts=%lld\n", frame->pts);
                        }
                        else if (d->next_pts != AV_NOPTS_VALUE) {
                            // 如果frame->pts不正常则使用上一帧更新的next_pts和next_pts_tb
                            // 转成{1, frame->sample_rate}
                            frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                        }
                        if (frame->pts != AV_NOPTS_VALUE) {
                            // 根据当前帧的pts和nb_samples预估下一帧的pts
                            d->next_pts = frame->pts + frame->nb_samples;
                            d->next_pts_tb = tb; // 设置timebase
                        }
                    }
                    break;
                }

                // 1.3. 检查解码是否已经结束，解码结束返回0
                if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    printf("avcodec_flush_buffers %s(%d)\n", __FUNCTION__, __LINE__);
                    d->component.flush();
                    return 0;
                }
                // 1.4. 正常解码返回1
                if (ret >= 0)
                    return 1;
            } while (ret != AVERROR(EAGAIN));   // 1.5 没帧可读时ret返回EAGIN，需要继续送packet
        }

        // 2 获取一个packet，如果播放序列不一致(数据不连续)则过滤掉“过时”的packet
        do {
            // 2.1 如果没有数据可读则唤醒read_thread, 实际是continue_read_thread SDL_cond
            if (packet_queue_packet_count(d->queue) == 0)  // 没有数据可读
                SDL_CondSignal(d->empty_queue_cond);// 通知read_thread放入packet
            // 2.2 如果还有pending的packet则使用它
            if (d->packet_pending) {
                av_packet_move_ref(&pkt, &d->pkt);
                d->packet_pending = 0;
            } else {
                // 2.3 阻塞式读取packet
                if (packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial) < 0)
                    return -1;
            }
            if(packet_queue_serial(d->queue) != d->pkt_serial) {
                // darren自己的代码
                printf("%s(%d) discontinue:queue->serial:%d,pkt_serial:%d\n",
                       __FUNCTION__, __LINE__, d->queue->serial, d->pkt_serial);
                av_packet_unref(&pkt); // fixed me? 释放要过滤的packet
            }
        } while (packet_queue_serial(d->queue) != d->pkt_serial);// 如果不是同一播放序列(流不连续)则继续读取

        // 3 将packet送入解码器
        if (pkt.data == flush_pkt.data) {//
            // when seeking or when switching to a different stream
            d->component.flush(); //清空里面的缓存帧
            d->finished = 0;        // 重置为0
            d->next_pts = d->start_pts;     // 主要用在了audio
            d->next_pts_tb = d->start_pts_tb;// 主要用在了audio
        } else {
            if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                int got_frame = 0;
                ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, &pkt);
                if (ret < 0) {
                    ret = AVERROR(EAGAIN);
                } else {
                    if (got_frame && !pkt.data) {
                        d->packet_pending = 1;
                        av_packet_move_ref(&d->pkt, &pkt);
                    }
                    ret = got_frame ? 0 : (pkt.data ? AVERROR(EAGAIN) : AVERROR_EOF);
                }
            } else {
                if (d->component.send(&pkt) == AVERROR(EAGAIN)) {
                    av_log(d->avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    d->packet_pending = 1;
                    av_packet_move_ref(&d->pkt, &pkt);
                }
            }
            av_packet_unref(&pkt);	// 一定要自己去释放音视频数据
        }
    }
}

//解码器销毁
static void decoder_destroy(Decoder *d) {
    av_packet_unref(&d->pkt);
    d->component.detach();
    d->owned_avctx.reset();
    d->avctx = nullptr;
}

static void frame_queue_unref_item(Frame *vp)
{
    if (vp->frame)
        av_frame_unref(vp->frame);
    avsubtitle_free(&vp->sub);
}
static void frame_queue_destory(FrameQueue *f);
//帧队列初始化（绑定数据包队列，初始化最大值）
static int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last)
{
    int i;
    f->rindex = 0;
    f->windex = 0;
    f->size = 0;
    f->rindex_shown = 0;
    f->pktq = pktq;
    for (i = 0; i < FRAME_QUEUE_SIZE; ++i)
        f->queue[i].frame = nullptr;
    f->owned_mutex.reset(SDL_CreateMutex());
    f->mutex = f->owned_mutex.get();
    if (!f->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    f->owned_cond.reset(SDL_CreateCond());
    f->cond = f->owned_cond.get();
    if (!f->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        f->owned_mutex.reset();
        f->mutex = nullptr;
        return AVERROR(ENOMEM);
    }
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    //为队列中所有的缓存帧预先申请内存
    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc())) {
            frame_queue_destory(f);
            return AVERROR(ENOMEM);
        }
    return 0;
}
//帧队列销毁
static void frame_queue_destory(FrameQueue *f)
{
    int i;
    for (i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        frame_queue_unref_item(vp);
        av_frame_free(&vp->frame);
    }
    f->owned_cond.reset();
    f->owned_mutex.reset();
    f->cond = nullptr;
    f->mutex = nullptr;
}
//帧队列信号
static void frame_queue_signal(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondBroadcast(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static Frame *frame_queue_peek(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static Frame *frame_queue_peek_next(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

static Frame *frame_queue_peek_last(FrameQueue *f)
{
    return &f->queue[f->rindex];
}

static Frame *frame_queue_peek_writable(FrameQueue *f)
{
    /* wait until we have space to put a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size &&
           !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[f->windex];
}

static Frame *frame_queue_peek_readable(FrameQueue *f)
{
    /* wait until we have a readable a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size - f->rindex_shown <= 0 &&
           !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static void frame_queue_push(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    if (++f->windex == f->max_size)
        f->windex = 0;
    f->size++;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static void frame_queue_next(FrameQueue *f)
{
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&f->queue[f->rindex]);
    SDL_LockMutex(f->mutex);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    f->size--;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

/* return the number of undisplayed frames in the queue */
static int frame_queue_nb_remaining(FrameQueue *f)
{
    return f->size - f->rindex_shown;
}

/* return last shown position */
static int64_t frame_queue_last_pos(FrameQueue *f)
{
    Frame *fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == f->pktq->serial)
        return fp->pos;
    else
        return -1;
}

static void decoder_abort(Decoder *d, FrameQueue *fq)
{
    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    if (d->decode_thread.joinable())
        d->decode_thread.join();
    packet_queue_flush(d->queue);
}




#endif // DATACTRL_H
