//
// Created by suhuamo on 2026/4/2.
//

#include "videoctrl.h"
#include <QCoreApplication>
#include<QFile>
#include<iostream>
#include<QFileInfo>
#include <cmath>
#include <QMutex>
#if defined(_WIN32)
#include <objbase.h>
#endif

// 取消 FFmpeg 的 isnan 宏定义,避免与 std::isnan 冲突
#ifdef isnan
#undef isnan
#endif

#define av_log_trace(fmt, ...) av_log(NULL, AV_LOG_TRACE, fmt, ##__VA_ARGS__)
#define av_log_info(fmt, ...) av_log(NULL, AV_LOG_INFO, fmt, ##__VA_ARGS__)
#define av_log_warning(fmt, ...) av_log(NULL, AV_LOG_WARNING, fmt, ##__VA_ARGS__)
#define av_log_error(fmt, ...) av_log(NULL, AV_LOG_ERROR, fmt, ##__VA_ARGS__)

// 是否开启丢帧，不开始为-1，开启为1
static int framedrop = -1;
// 是否开启不限制缓存，-1不开启，开启为1
static int infiniteBuffer = -1;
/*
 * mark：这里应该是外部传入的，
 * 作用：设置播放总时长，AV_NOPTS_VALUE 为无限。假设设置 1 min，那么视频播放一分钟后自动结束，不管你这个视频有多长都没用【当然实际上单位是微秒】
 */
int64_t g_play_total_duration = AV_NOPTS_VALUE;
/*
 * mark：这里应该是外部传入的，
 * 作用：从视频的第几秒开始播放，0 代表从第 0 s开始播放。如果 = 10 那么代表从第 10 s开始播放【当然实际上单位是微秒】
 */
int64_t g_play_start_time = 0;
/*
 * mark: 这里应该是外部传入的，而不是写死的,现在写0代表我没有指定，实际内容应该写，"v"、"v:0"、“v:1" 这样
 * 作用：一个视频可能有两个视频流，就像画画的图层一样，可以有两个图层，但是只能播放一个（图层只能显示一个被我们看到）。默认我们看到的是第一个视频流（最上面的图层），但是我们可以指定我们真正想看到哪个流（哪个图层）
 * 所以视频其实可以夹带私货，你上传一个视频审核看到好像没问题，然后人家下载下来后进行解析，就可以解析到你隐藏起来的小视频
 * 设置操作如：wanted_stream_spec[AVMEDIA_TYPE_VIDEO] = "v:1";
 */
const char *wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};
/*
 * mark：这里应该是外部传入的，
 * 强制使用指定的解码器
 */
const char* forced_codec_name = nullptr;
// 界面显示锁
QMutex g_show_rect_mutex;
// 音频回调时的时间戳
static int64_t g_audio_callback_time;
/*
 * mark：默认是5，表示每次快进快退 5s
 */
const int SEEK_INCR = 5;
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

static FILE *log_file = NULL;
// 自定义日志回调
static void log_callback(void *ptr, int level, const char *fmt, va_list vl) {
    // 小于 av_log_get_level 不输出
    if (level <= av_log_get_level()) {
        // 记录当前时间
        SYSTEMTIME st;
        GetLocalTime(&st);
        char time_buf[64];
        snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        // 输出到文件
        if (log_file) {
            fprintf(log_file, "[%s] ", time_buf);
            vfprintf(log_file, fmt, vl);
            fflush(log_file);
        }

        // 输出到窗口
        fprintf(stderr, "[%s] ", time_buf);
        vfprintf(stderr, fmt, vl);
    }
}

int decode_interrupt_cb(void *ctx) {
    VideoState *is = (VideoState *)ctx;
    return is->abort_request;
}

/*
 * 硬件解码的格式协商回调：仅在设备上下文提供的像素格式出现在解码器候选列表中时使用硬件格式，
 * 否则回退到第一个（软件）格式，保证硬件不可用时仍能正常解码。
 */
static enum AVPixelFormat pick_hardware_pixel_format(AVCodecContext *ctx, const enum AVPixelFormat *pixFmts)
{
    const HardwareDecoderDevice *device = static_cast<const HardwareDecoderDevice *>(ctx->opaque);
    if (device && device->isActive()) {
        for (const enum AVPixelFormat *pixelFormat = pixFmts; *pixelFormat != AV_PIX_FMT_NONE; ++pixelFormat) {
            if (*pixelFormat == device->pixelFormat()) {
                return *pixelFormat;
            }
        }
    }
    return pixFmts[0];
}

/*
 * 解析 PVP_HW_DECODE 环境变量：
 * - 未设置：启用硬件解码，使用平台默认设备类型；
 * - off / 0 / none：禁用硬件解码；
 * - 其他取值：按 FFmpeg 设备名（如 d3d11va、cuda）启用指定设备。
 * 返回 false 表示禁用硬件解码。
 */
static bool resolveHardwareDecodePreference(AVHWDeviceType *preferredType)
{
    const char *requested = getenv("PVP_HW_DECODE");
    if (requested) {
        if (!strcmp(requested, "0") || !strcmp(requested, "off") || !strcmp(requested, "none")) {
            return false;
        }
        const AVHWDeviceType named = av_hwdevice_find_type_by_name(requested);
        if (named != AV_HWDEVICE_TYPE_NONE) {
            *preferredType = named;
            return true;
        }
        av_log_warning("unknown PVP_HW_DECODE value '%s', using platform default device\n", requested);
    }
#if defined(_WIN32)
    *preferredType = AV_HWDEVICE_TYPE_D3D11VA;
#elif defined(__APPLE__)
    *preferredType = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#else
    *preferredType = AV_HWDEVICE_TYPE_VAAPI;
#endif
    return true;
}

int is_realtime(AVFormatContext *s) {
    if (!strcmp(s->iformat->name, "rtp")
        || !strcmp(s->iformat->name, "rtsp")
        || !strcmp(s->iformat->name, "sdp"))
        return 1;
    /*
     * 如果允许为空的话，那 s->pb 是否为空跟网络流也没关系呀，难道是本地文件 s->pb 就可以为空吗
     */
    if (s->pb && (!strncmp(s->url, "rtp:", 4)
                  || !strncmp(s->url, "sdp:", 4)))
        return 1;
    return 0;
}

VideoCtrl::VideoCtrl(QObject *parent):
    PlaybackEventSource(parent),
    m_init(false),
    m_play_loop(false),
    m_cur_stream(nullptr),
    m_screen_width(0),
    m_screen_height(0),
    m_frame_width(0),
    m_frame_height(0),
    m_is_full_screen(false),
    m_audio_speed_convert(nullptr),
    m_stop_emitted(false),
    m_video_open(false),
    m_frame_sar({0, 1}),
    m_frame_flip_v(false),
    m_last_frame(nullptr),
    m_idle_loop(false),
    m_user_stop(false){
}

bool VideoCtrl::init() {
    if (m_init)
        return true;

    if (ConnectionSignalSlots() == false)
    {
        return false;
    }

    // 设置日志级别（可选）
    // av_log_set_level(AV_LOG_TRACE);
    // 打开日志文件
    log_file = fopen("PVideo-player.log", "w");
    if (!log_file) return -1;

    // 注册回调
    av_log_set_callback(log_callback);

    // 初始化 SDL（视频 + 事件系统）
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        std::cerr << "Could not initialize SDL - " << SDL_GetError() << std::endl;
        return false;
    }

    m_screen_width = 1920;
    m_screen_height = 1080;

    m_init = true;
    return true;
}

bool VideoCtrl::ConnectionSignalSlots()
{
    connect(this, &VideoCtrl::SigStop, &VideoCtrl::OnStop);
    return true;
}

VideoCtrl::~VideoCtrl() {
    // 停止播放循环
    m_play_loop = false;
    // 停止空闲事件循环
    m_idle_loop = false;
    // 空闲循环会在 SDL_WaitEventTimeout 的 100ms 超时后检查 m_idle_loop 自然退出
    if (m_play_loop_thread.joinable()) {
        m_play_loop_thread.join();
    }

    // 清理视频纹理、渲染器和窗口
    m_video_output_resources.reset();
    // 清理最后一帧引用
    m_last_frame.reset();
    //清理日志文件
    if (log_file) {
        fclose(log_file);
        log_file = nullptr;
    }
    // 清理 SDL
    SDL_Quit();
}

void VideoCtrl::start_play(QString filename, WId play_wid) {

    /*
     * 先递增会话代际：旧会话线程（正在被 join 的 read/loop 线程）在停止过程中发出的事件
     * 会立即被判定为过期事件，不再投递给播放服务，避免污染新文件的播放状态。
     */
    m_session.begin();

    m_play_loop = false;

    // 停止空闲事件循环（如果正在运行）
    m_idle_loop = false;
    // 空闲循环会在 SDL_WaitEventTimeout 的 100ms 超时后检查 m_idle_loop 自然退出

    m_user_stop = true;

    if (m_play_loop_thread.joinable()) {
        m_play_loop_thread.join();
    }

    emit SigStartPlay(filename);

    m_play_wid = play_wid;
    m_stop_emitted = false; // 重置停止标志
    m_current_file = filename;
    m_user_stop = false; // 重置用户停止标志


    char filename_c[1024] = {};
    sprintf(filename_c, "%s", filename.toStdString().c_str());

    VideoState *is = stream_open(filename_c);
    if (!is) {
        av_log_error("Failed to initialize!\n");
        m_cur_stream = nullptr;
        emit SigError(tr("无法打开媒体：%1").arg(filename));
        return;
    }

    m_cur_stream = is;
    m_play_loop_thread = std::thread(&VideoCtrl::loop_thread, this, is);
    return;
}

VideoCtrl *VideoCtrl::create(QObject *parent)
{
    VideoCtrl *controller = new (std::nothrow) VideoCtrl(parent);
    if (!controller) {
        return nullptr;
    }
    if (!controller->init()) {
        delete controller;
        return nullptr;
    }
    return controller;
}

bool VideoCtrl::isCurrentSession(const VideoState *is) const
{
    return is != nullptr && m_session.matches(is->session_epoch);
}

PlaybackBackendBundle CreateDefaultPlaybackBackend()
{
    // 组合根调用：后端由 QApplication 持有，随应用退出统一销毁
    VideoCtrl *controller = VideoCtrl::create(QCoreApplication::instance());

    PlaybackBackendBundle bundle;
    bundle.events = controller;
    bundle.backend = controller;
    return bundle;
}

VideoState * VideoCtrl::stream_open(const char *filename) {
    VideoState *is = new (std::nothrow) VideoState{};
    if (!is) {
        return nullptr;
    }

    // mark：记录当前播放的流序号，实际上 last_video_stream 和 video_stream 只需要一个就可以了。
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;

    is->filename = filename;
    // SDL 音频回调通过该反向引用取回拥有者实例
    is->controller = this;
    // 记录本会话所属代际，供各工作线程判定事件是否仍然有效
    is->session_epoch = m_session.current();

    // 窗口起始位置
    is->ytop = 0;
    is->xleft = 0;

    /*
     * mark: keep_last
     * keep_last：保存frame队列中最后一张图片不销毁
     * rindex_shown：是否显示过一张图片了；或者说已经从队列用过一个frame了。当这个值为1了之后，就代表从frame中get的时候一定能获取到图片了。【当然，实际上这个可以不限制1，可以设置保存多个图片。但没什么意义，一般只保存一张就够了】
     * 首先声明，视频 frame 的 keep_last 配置一定要开启，否则不会显示图片。这个不是说 keep_last 的功能变成了是否控制图片显示，而是顺带完成了另一个功能，这个在另一个地方有讲解。
     */
    if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
        goto fail;
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) <0)
        goto fail;

    if (packet_queue_init(&is->videoq) < 0)
        goto fail;
    if (packet_queue_init(&is->audioq) < 0)
        goto fail;

    is->owned_continue_read_thread.reset(SDL_CreateCond());
    is->continue_read_thread = is->owned_continue_read_thread.get();
    if (!is->continue_read_thread)
        goto fail;

    // 可选视频滤镜链，通过环境变量提供（例如 PVP_VIDEO_FILTER=hflip）；未设置时不启用
    if (const char *filterDescription = getenv("PVP_VIDEO_FILTER")) {
        is->filter_description = filterDescription;
    }

    init_clock(&is->vidclk, &is->videoq.serial);
    init_clock(&is->audclk, &is->audioq.serial);
    is->audio_clock_serial = -1;

    // 当前音量（0.0~1.0 的比值）换算为 SDL 音量刻度
    is->audio_volume = m_settings.volumeFor(SDL_MIX_MAXVOLUME);

    //    启动播放的时候需要更新下界面ui的播放状态
    emit SigPauseStat(is->paused);
    is->av_sync_type = AV_SYNC_AUDIO_MASTER;

    is->read_tid = std::thread(&VideoCtrl::read_thread, this, is);

    return is;
fail:
    stream_close(is);
    return nullptr;
}

void VideoCtrl::init_clock(Clock *c, int *queueSerial) {
    m_clock_controller.initialize(c, queueSerial);
}

void VideoCtrl::set_clock(Clock *c, double pts, int serial) {
    m_clock_controller.set(c, pts, serial);
}

void VideoCtrl::set_clock_speed(Clock *c, double speed) {
    /*
     * mark：先通过 get_clock 获取当前时钟值并重新 set_clock，更新 last_updated 和 pts_drift，
     * 然后再修改 speed。这样 get_clock 在之后的时间推进中就能正确使用新的 speed 值。
     * 如果不先 set_clock，那么 speed 改变后，用旧的 pts_drift 和 last_updated 算出来的时钟值会跳变。
     */
    m_clock_controller.setSpeed(c, speed);
}

void VideoCtrl::set_clock_at(Clock *c, double pts, int serial, double time) {
    m_clock_controller.setAt(c, pts, serial, time);
}

double VideoCtrl::get_master_clock(VideoState *is) {
    double val;
    switch (get_master_sync_type(is)) {
    case AV_SYNC_VIDEO_MASTER:
        val = get_clock(&is->vidclk);
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = get_clock(&is->audclk);
        break;
    default:
        val = get_clock(&is->audclk);
        break;
    }
    return val;
}

double VideoCtrl::get_clock(Clock *c) {
    return m_clock_controller.value(c);
#if 0
    /* {
        /*
         * mark：在正常情况下，应该是直接 return c->pts_drift + time
         * 有倍数播放的时候，假设是一倍速，那么 (1.0 - c->speed) = 0，那么还是上面的结果，即 return c->pts_drift + time
         * 但如果不是正常速度，比如2倍数，那么 (1.0 - c->speed) = -1，
         * 而 (time - c->last_updated) 代表的是自上次更新以来经过的时间，
         * - (time - c->last_updated) * (1.0 - c->speed) 其实就是又加上了一倍上次更新以来经过的时间，
         * 如果实际过去了1秒，但在2倍速播放下，媒体内容应该前进2秒，所以时钟会以两倍的速度递增，反映出媒体内容的快速推进
         *
         */
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
    */
#endif
}

void VideoCtrl::read_thread(VideoState *is) {
    AVFormatContext *ic = nullptr;
    int ret = 0;
    int stream_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int pkt_ts = 0;

    if (!wait_mutex) {
        av_log_error("SDL_CreateMutex failed %s\n", SDL_GetError());
        goto fail;
    }

    memset(stream_index, -1, sizeof(stream_index));
    is->eof = 0;
    if (!is->demux_reader.open(is->filename.c_str(), decode_interrupt_cb, is)) {
        av_log_error("cannot open %s\n", is->filename.c_str());
        ret = -1;
        goto fail;
    }
    ic = is->demux_reader.context();

    // 输出视频信息，可以不写
    av_dump_format(ic, 0, is->filename.c_str(), 0);

    is->ic = ic;

    // mark: avformat_find_stream_info 执行后文件指针探针会一直往后移动，可能已经移动到了末尾，eof就会被设置为1，即被认为文件已经结束。但实际上我们还没开始读取数据播放视频呢，所以我们手动将eof置为0，防止探针被移动到了文件末尾后将eof置为1的情况
    if (ic->pb)
        ic->pb->eof_reached = 0;

    /*
     * mark：前后两帧的时间间隔，AVFMT_TS_DISCONT 参数代表允许时间戳不连续。即第一帧的pts假设的 0.1 s，第二帧的pts假设是 8s，这个很明显有问题对吧。
     * 但是允许时间戳不连续，那么这个情况就是正常的，认为两帧之间的 duration 这么长没问题。那就认为第一帧就可以播放8s，直到第8s的时候才播放第二帧，因为第二帧的pts是8s。
     * 这个 10.0 就是经验值，一般我们认为不连续10s就够了，再多已经不能算不连续了，pts肯定有问题了，或者是seek了。
     * 而这个 3600.0s = 1小时，为什么要这么设置？因为如果 不允许时间戳不连续，那么就是每个时间戳其实都是连续的，那么后面这个值随便设置什么都可以了，只是为了避免可能产生的问题，所以这里设置成1小时而已，你写成 1s也无所谓。
     * 这个值的作用：
     * 1. 计算上一帧视频画面的播放时长。一般拿当前帧的 pts - 上一帧的 pts 的值作为上一帧的播放时长。如果差值大于了 max_frame_duration，那么这两帧肯定就是不连续的了，那么上一帧的播放时长就取自己的 duration 字段就行了。
     *    如果差值小于 max_frame_duration，就认为这两帧是连续的，那么上一帧的 pts 和当前帧的 pts就应该按固定值播放，那么上一帧的播放时长就取 pts2 - pts1。
     *      举个例子：比如没seek的时候，第一帧的pts是 10，第二帧的pts是 30，那么很明显第一帧的播放时长就是 30-10 = 20。如果seek了，seek前的最后一帧pts是40，seek后的第一帧pts是1000，那总不可能这一帧播放 1000-40 = 960 时长吧，肯定就取自己的duration播放了
     * 2. 音视频同步的时候，视频画面需要同步音频播放，一般时钟过快或者过慢，我们会选择人为的调整下画面帧的显示时长，比如时钟快了，那么本来这一帧显示 20 pts，我们可能会显示 22 pts。如果时钟慢了，本来这一帧显示 20 pts，我们可能会显示 18 pts。
     *    而 max_frame_duration 在这里使用的时候同理，如果超过了 max_frame_duration，我们就认为不是时钟快或者慢了，而是 seek 了，就做额外处理了，画面帧该显示 20pts，我们就显示 20 pts的时长了。
     */
    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    is->realtime = is_realtime(ic);

    if (isCurrentSession(is)) {
        emit SigVideoTotalSeconds(ic->duration / AV_TIME_BASE);
    }

    // 如果有自己期望的视频流，那么这里就先尝试使用期望的视频流
    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        /*
         * mark：当 discard 标志被设置为 AVDISCARD_ALL 时，这个流会被丢弃，不会被解码，也不会被显示。即 ffmpeg 会忽略对这个流的操作，可以降低CPU的消耗。比如 find_stream 的时候，遍历的时候这个流就不会参与到遍历，时间复杂度就小了。
         * 在这里我们先强制将所有的流丢弃掉，在 stream_component_open 的时候，我们真正选用哪个流，我们会将这个标志给改回来的。
         */
        st->discard = AVDISCARD_ALL;
        if (type > 0 && wanted_stream_spec[type] && stream_index[type] == -1) {
            // 判断当前流是不是期望的视频流，假设 wanted_stream_spec[type] 写的是 v:1，那么就代表匹配的是第二个视频流，那么第一个视频流虽然是视频流，但是它不是期望的视频流，if就失败。只有第二个视频流这里if才是true
            if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0) {
                stream_index[type] = i;
            }
        }
    }


    for (int i = 0; i < AVMEDIA_TYPE_NB; i++) {
        // 如果指定了这个类型的流需要找到指定的序号流，但是没找到，那么标记这个类型的流不播放了
        if (wanted_stream_spec[(AVMediaType)i] && stream_index[(AVMediaType)i] == -1) {
            av_log_error("cannot find spec stream");
            stream_index[(AVMediaType)i] = INT_MAX;
        }
    }

    /*找到最合适的视频流，这里其实解决了三个问题：
     * 1. 如果没有指定流，那么 stream_index[AVMEDIA_TYPE_VIDEO] 为-1，当 av_find_best_stream 传入 -1 的时候，其实默认是找找第一个视频流
     * 2. 如果指定流视频流，分两种情况
     *  1. 如果找到了视频流，假设为 x，那么此时 stream_index[AVMEDIA_TYPE_VIDEO] = x，当 av_find_best_stream 传入 x 的时候，就是从第 x 流开始找第一个能用的视频流，很明显 x 这个流就是能用，那么就会马上返回 x
     *  2. 如果没有找到视频流，那么 stream_index[AVMEDIA_TYPE_VIDEO] = INT_MAX，当 av_find_best_stream 传入 INT_MAX 的时候，必然会返回-1，因为会从 INT_MAX 个流开始找，很明显视频不可能有 INT_MAX 个流，必然返回-1，即找不到流
     *      所以，如果想要在指定流且找不到流的情况下，不退出程序，那么删掉上面的  stream_index[(AVMediaType)i] = INT_MAX; 即可
     */
    stream_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, stream_index[AVMEDIA_TYPE_VIDEO], -1, nullptr, 0);
    stream_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, stream_index[AVMEDIA_TYPE_AUDIO], stream_index[AVMEDIA_TYPE_VIDEO], nullptr, 0);

    // 我觉得这个方法应该放在 streamOpen 中，而不是 ReadThread 中
    if (stream_index[AVMEDIA_TYPE_AUDIO] >= 0)
        stream_component_open(is, stream_index[AVMEDIA_TYPE_AUDIO]);

    if (stream_index[AVMEDIA_TYPE_VIDEO] >= 0)
        stream_component_open(is, stream_index[AVMEDIA_TYPE_VIDEO]);

    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log_error("failed to open fail, not find stream");
        ret = -1;
        goto fail;
    }

    // 如果是实时流，那么无限缓冲
    if (infiniteBuffer < 0 && is->realtime)
        infiniteBuffer = 1;

    // 从视频中读取包
    while (true) {
        if (is->abort_request)
            break;
        // 当暂停或者恢复播放的时候，会触发这个判断
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            /*
             * mark：暂停播放的时候暂停读取数据，可以节省CPU的消耗【不加这里的判断处理也没事，只是为了优化而已】
             * 同时对于网络流来说，需要给他们发送一个信号，标识暂停播放或者恢复播放【当然这些是由ffmpeg内部实现的，不需要我们关心】
             */
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
        /*
         * mark: seek 的功能只需要这部分代码就实现了，其他地方不用改，因为其他地方就是正常播放队列里面的数据，只要我们往队列里面填充的是seek后应该显示的数据，那就实现了seek了
         */
        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            /*
             * mark：这里的 seek_min 和 seek_max 计算是为了优化，当然可以直接写 INT64_MIN 和 INT64_MAX，但是这样的话 ffmpeg 可能会查找整个文件，会浪费效率，而且精度可能偏移有点多
             * 假设现在是 100s，要跳转到 85s，如果不设置偏移量，那么就是在 INT64_MIN 和 INT64_MAX 范围内找关键帧，这样会很慢，而且可能不太准确，比如找打 82s，或者90s的帧画面了
             * 假设有偏移量， 100s，要跳转到 85s，就是向后退，那么 seek_min 还是取 INT64_MIN，因为在当前位置之前，肯定需要往前找关键帧，而 seek_max 就是 100 + rel - 2，假设就是 87【因为 rel 小于0】，
             * 这样查找的范围就是 0 ~ 87，首先效率变快了，其次找到的关键帧位置更精准，我们要跳转到 85，那么最终跳转到的会是 0~87 之间的一个值，
             * 而不会在 87 更加往后的位置了，这样至少会比 0~文件末尾的位置判断和跳转更准确点
             * 其中+2，-2是为了边界保护，避免刚好卡在临界值导致找不到关键帧，这个我也不太清楚什么情况会导致这样，但是毕竟ffplay这么写了
             *
             */
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;

            ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            // seek成功则清空队列里面的数据,这样队列里面拿到的最新数据就是seek后的数据
            if (ret < 0) {
                av_log_error("%s: error while seeking\n", is->ic->filename);
            } else {
                if (is->audio_stream >= 0) {
                    packet_queue_flush(&is->audioq);
                    packet_queue_put(&is->audioq, &flush_pkt);
                }
                if (is->video_stream >= 0) {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
            }
            // 标记seek完成
            is->seek_req = 0;
            // 重新将图片写入一次队列中，感觉这个没有必要吧，frame队列里面不是有这个图片吗
            is->queue_attachments_req = 1;
            // 由于seek了，可能文件探针到文件末尾，导致eof被改为1了，所以我们手动将eof改为0，确保没问题
            is->eof = 0;
            // 如果现在是暂停情况下，那么我们需要手动播放下一帧，即显示seek位置的图片，不然暂停的时候点击seek，会发现画面没有变动，因为暂停了嘛
            if (is->paused)
                step_to_next_frame(is);

        }
        // 这个判断其实不是给视频用的，而是给音频用的，可见 stream_component_open 中赋值该值时的解释
        if (is->queue_attachments_req) {
            if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                AVPacket copy;
                if ((ret = av_packet_ref(&copy, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, &copy);
                packet_queue_put_nullpacket(&is->videoq, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }
        /* if the queue are full, no need to read more */
        // 限制pkt的缓存数量
        if (infiniteBuffer<1 &&
            (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
             || (stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq) &&
                 stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq)))) {
            /*
             * mark：wait 10 ms
             * 这段代码的作用是短暂休眠读取线程，它是一个线程同步机制，具体来说：
             * 锁定互斥锁 (SDL_LockMutex)：防止其他线程同时访问共享资源
             * 等待条件变量 (SDL_CondWaitTimeout)：在 continue_read_thread 条件变量上等待最多10毫秒
             * 解锁互斥锁 (SDL_UnlockMutex)：释放锁，允许其他线程访问资源
             * 这个机制出现在两种情况下：
             * 1. 队列已满时：当音频、视频和字幕队列的数据量超过 MAX_QUEUE_SIZE 或者流已经有足够数据包时，线程会休眠10毫秒，等待播放器消费一些数据后再继续读取
             * 2. 读取错误或到达文件末尾时：当 av_read_frame 返回错误时（但不是文件结束错误），线程会休眠10毫秒后继续尝试读取【即下面的那个地方】
             */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }

        // 如果在非暂停状态下，没有视频流或者解码器解码结束了，就代表视频文件读取完毕了，结束播放
        if (!is->paused && (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))
            && (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0))) {
            /*
             * mark：使用标志位确保 SigStop 只发送一次，防止在 abort_request 被设置之前重复触发。
             * 流程：这里发射 SigStop → OnStop 设置 m_play_loop=false → loop_thread 退出循环 → do_exit 设置 abort_request=1 → 这里 while 循环检测到 abort_request 后 break
             */
            if (!m_stop_emitted && isCurrentSession(is)) {
                m_stop_emitted = true;
                emit SigStop();
            }
            continue;
        }

        ret = is->demux_reader.read(pkt);
        if (ret < 0) {
            // 如果文件读取结束了？
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                /*
                * mark：这里放一个空包是为了冲刷解码器，将解码器中剩余的数据读取出来。这里可能会被反复调用，但是不会有问题。
                */
                if (is->video_stream >= 0) {
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                }
                if (is->audio_stream >= 0) {
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                }
                is->eof = 1;
            }
            // IO坏了肯定要退出的
            if (ic->pb && ic->pb->error)
                break;
            /*
             * mark：如上相同地方
             */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        } else {
            is->eof = 0;
        }

        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        /*
         * pkt_in_play_range 作用：判断当前包是否在播放范围之内
         * 1. 如果 g_setting_duration == AV_NOPTS_VALUE：代表需要播放所有数据，那么 pkt_in_play_range 就为 true。
         * 2. 如果 g_setting_duration ！= AV_NOPTS_VALUE：
         *      假设开始时间是 10s，g_setting_start_time 是 2 min，那么就代表我们需要播放 0:10 ~ 2:10 分的数据，那么当前包的 pts 必须得在0:10 ~ 2:10 范围内，否则这个包不在范围内，我们又不会去播放它，要它干吗，所以 就不会进入这个 if。
         */
        pkt_in_play_range = g_play_total_duration == AV_NOPTS_VALUE ||
                            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                                        av_q2d(ic->streams[pkt->stream_index]->time_base) -
                                    (double)(g_play_start_time != AV_NOPTS_VALUE ? g_play_start_time : 0) / 1000000
                                <= ((double)g_play_total_duration / 1000000);

        if (pkt->stream_index == is->video_stream && pkt_in_play_range &&
            !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&is->videoq, pkt);
        } else if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        }
        else {
            av_packet_unref(pkt);
        }

    }

    ret = 0;
fail:
    if (ret != 0) {
        is->demux_reader.close();
        is->ic = nullptr;
    }
    if (ret != 0) {
        SDL_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return;
}

int VideoCtrl::stream_component_open(VideoState *is, int stream_index) {
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    const AVCodec *codec;
    AVDictionaryEntry *t = nullptr;
    AVDictionary *opts = nullptr;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    bool hardwareEnabled = false;
    AVHWDeviceType hardwareDeviceType = AV_HWDEVICE_TYPE_NONE;
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = avcodec_alloc_context3(nullptr);
    if (!avctx)
        return AVERROR(ENOMEM);
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;

    // 设置解码器时间基，否则 avctx->pkt_timebase 默认为 {0, 1}，导致 av_rescale_q 计算时除以0或得到错误结果。
    av_codec_set_pkt_timebase(avctx, ic->streams[stream_index]->time_base);
    codec = avcodec_find_decoder(avctx->codec_id);

    opts = nullptr;
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    /*
     * mark：这里少了一行代码开启帧引用计数，作用是防止多个frame创建多个buf，同一张图片的frame应该用的是同一个buf，而不是用多个buf。
     * FFmpeg 4.0+ 默认启用帧引用计数，不需要再设置 refcounted_frames； av_dict_set(&opts, "refcounted_frames", "1", 0);
     * 如果是 4.0 之前的，那么还需要调用  av_dict_set(&opts, "refcounted_frames", "1", 0); 这个方法
     *
     */
    // 音频和视频需要开启，如果是字幕则不需要开启
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);

    /*
     * 视频流优先尝试硬件解码：先按解码器声明的硬件能力创建设备上下文，
     * 设备或解码器不可用时自动回退到软件解码，保证行为与原来一致。
     */
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO && codec
        && resolveHardwareDecodePreference(&hardwareDeviceType)) {
        hardwareEnabled = is->hw_device.initializeForDecoder(codec, hardwareDeviceType);
        if (hardwareEnabled) {
            avctx->hw_device_ctx = av_buffer_ref(is->hw_device.context());
            avctx->opaque = &is->hw_device;
            avctx->get_format = pick_hardware_pixel_format;
        }
    }
    ret = avcodec_open2(avctx, codec, &opts);
    if (ret < 0 && hardwareEnabled) {
        av_log_warning("hardware decoder open failed, falling back to software decoding\n");
        hardwareEnabled = false;
        is->hw_device.reset();
        // avcodec_open2 失败后上下文不可复用，必须释放并重新构建
        avcodec_free_context(&avctx);
        avctx = avcodec_alloc_context3(nullptr);
        if (!avctx) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        if ((ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar)) < 0)
            goto fail;
        av_codec_set_pkt_timebase(avctx, ic->streams[stream_index]->time_base);
        ret = avcodec_open2(avctx, codec, &opts);
    }
    if (ret < 0)
        goto fail;
    // 判断是否有未识别的参数
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log_error("Option %s not found\n", t->key);
        ret =  AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }

    // 标记现在文件内容都是可读的【防止刚刚文件读取到末尾了之后 eof 被设置为 1 了，当然这个刚刚不一定是刚刚写的代码，而是不知道哪个地方的刚刚】
    is->eof = 0;    /*
     * mark：就在刚刚我们可以看到所有流都被执行了 st->discard = AVDISCARD_ALL; 即被标记为丢弃了。而我们在这里决定了我们要使用这个流，所以我们需要恢复这个流，所以要将标记改回来。
     */
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    // 开始音频、视频的解码相关操作=============
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        sample_rate = avctx->sample_rate;
        nb_channels = avctx->channels;
        channel_layout = avctx->channel_layout;
        if ((ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0) {
            goto fail;
        }
        // SDL缓冲区大小
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        /*
             * mark：初始化SDL buf需要用的到参数
             * audio_buf_size：本次音频buf的数据大小
             * audio_buf：本次音频buf的指针
             * audio_buf_index：buf的索引，即读取到buf的哪个位置了
             */
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;
        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

        is->last_audio_stream = is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
        is->auddec.owned_avctx.reset(avctx);
        is->auddec.avctx = is->auddec.owned_avctx.get();
        if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
        packet_queue_start(is->auddec.queue);
        is->auddec.decode_thread = std::thread(&VideoCtrl::audio_thread, this, is);
        // 开始播放
        m_audio_output.pause(false);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->last_video_stream = is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];
        decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);
        is->viddec.owned_avctx.reset(avctx);
        is->viddec.avctx = is->viddec.owned_avctx.get();
        // 每次创建一个解码器，就调用一次 decoder_init，即插入一个 flush_pkt,实际上是为了创建一个 serial
        packet_queue_start(is->viddec.queue);
        is->viddec.decode_thread = std::thread(&VideoCtrl::video_thread, this, is);
        /*
             * mark：这个字段其实是跟音频相关的，设置了这个之后一般会加上 (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) 充当if判断，看是否需要读取封面图片。而封面图片是给音频用的，所以这个为 true 的情况下，一般就是 mp3 + 1张图片 生成的 mp4文件。
             * 所以结果为 true，那么就会读取 is->video_st->attached_pic【这个就是封面图片】 显示，不会再读取其他pkt数据了，当然其实也不会有其他pkt数据。
             * 而如果这个视频文件不是单纯的 mp3 + 1张图片的这种文件，而是普通的 音频+视频文件，那么 is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC 这个就一定是 false，即一定没有封面图片，我们正常读取视频包数据然后放入队列即可。
             * 在 av_read_frame 的后面我们可以发现用了 !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) 来判断是否能够 packet_queue_put 进去。就是因为如果 (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) 不满足了，就代表一定是正常视频流，正常读取就可以了。
             * 而如果 !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) 为真了，就代表这个视频流一定有问题，就是一个单纯的封面图片而已，我们就不需要再处理它的pkt了。它的封面图片在另一个地方我们已经读取写入到 pkt 中反复播放了。
             */
        is->queue_attachments_req = 1;
        break;
    default:
        break;
    }
    goto out;
fail:
    avcodec_free_context(&avctx);
out:
    av_dict_free(&opts);
    return 0;
}

void sdl_audio_callback(void *opaque, Uint8 *stream, int len) {
    VideoState *is = (VideoState*)opaque;

    int audio_size, len1;
    VideoCtrl *controller = is->controller;
    if (!controller) {
        SDL_memset(stream, 0, static_cast<size_t>(len));
        return;
    }
    /*
     * mark: 我们需要在while循环前设置这个时间
     */
    g_audio_callback_time = av_gettime_relative();

    while (len > 0) {
        // 如果音频buf已经用完，那么就重新获取数据
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = controller->audio_decode_frame(is);
            // 如果获取数据失败，那么就设置一个默认静音数据
            if (audio_size < 0) {
                is->audio_buf = nullptr;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
            } else {
                is->audio_buf_size = audio_size;
            }
            // 获取到新数据了，索引从头开始
            is->audio_buf_index = 0;
            /*
             * mark: 对于音频来说，变速相关处理其实是跟给的数据有关，你给的数据是倍数后的数据，那么播放的时候就是倍数了，即播放的地方不需要做额外的处理的，
             * 所以我们这边要对给的数据进行倍数处理，处理结束后，音频倍数就
             */
            // 倍数是否发生了改变，如果发生了改变，那么 sonic 对象需要重新创建，因为 sonic 的倍数是固定的
            if (controller->get_playback_change()) {
                // 如果有sonic对象，那么需要先销毁 sonic---因为这个是老的配置，现在需要给sonic新的配置
                if (controller->m_audio_speed_convert) {
                    sonicDestroyStream(controller->m_audio_speed_convert);
                }
                // 创建新sonic
                controller->m_audio_speed_convert = sonicCreateStream(controller->get_target_frequency(), controller->get_target_channels());
                // 设置变速系数
                sonicSetSpeed(controller->m_audio_speed_convert, controller->get_playback_rate());
                sonicSetPitch(controller->m_audio_speed_convert, 1.0);
                sonicSetRate(controller->m_audio_speed_convert, 1.0);
                // 标记倍数改变事件处理结束 -- sonic 对象重新创建结束
                controller->set_playback_change(false);
            }
            /*
             * mark: 如果 Sonic 对象为空且需要变速播放，则立即创建
             * 比如启动的播放的时候倍数就是非常规倍数，此时 is_normal_playback_rate 是 false，但是上面的 get_playback_change 是fasle，并没有创建 sonic 对象，所以我们可以拦截到这种异常情况
             * 或者切换音频流的时候，情况和上面是一样的
             */
            if (!controller->is_normal_playback_rate() && !controller->m_audio_speed_convert) {
                controller->m_audio_speed_convert = sonicCreateStream(controller->get_target_frequency(), controller->get_target_channels());
                sonicSetSpeed(controller->m_audio_speed_convert, controller->get_playback_rate());
                sonicSetPitch(controller->m_audio_speed_convert, 1.0);
                sonicSetRate(controller->m_audio_speed_convert, 1.0);
            }
            // 是否需要做倍数处理，如果倍数不是1倍，且有音频数据，那么使用sonic对 audio_buf 进行转换得到倍数后的新音频数据
            if (!controller->is_normal_playback_rate() && is->audio_buf) {
                // 计算 swr_convert 实际返回了多少个样本
                int actual_out_samples = is->audio_buf_size / (is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt));

                int ret = 0, out_size = 0, nb_samples = 0, sonic_samples = 0;
                if (is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT) {
                    ret = sonicWriteFloatToStream(controller->m_audio_speed_convert, (float*)is->audio_buf, actual_out_samples);
                } else if (is->audio_tgt.fmt == AV_SAMPLE_FMT_S16) {
                    ret = sonicWriteShortToStream(controller->m_audio_speed_convert, (short*)is->audio_buf, actual_out_samples);
                } else {
                    av_log_error("sonic not support fmt:%d", is->audio_tgt.fmt);
                }
                nb_samples = sonicSamplesAvailable(controller->m_audio_speed_convert);
                out_size = nb_samples * av_get_bytes_per_sample(is->audio_tgt.fmt) * is->audio_tgt.channels;
                // 为 audio_buf1 分配足够的内存空间
                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if (!is->audio_buf1) {
                    av_log_error("Failed to allocate audio_buf1 for sonic processing\n");
                    continue;
                }
                // 如果将原始的音频数据 audio_buf 写入 sonic 失败，那么也就不需要从 sonic 读取处理后的音频数据了 —— 毕竟已经失败了
                if (ret) {
                    if (is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT) {
                        sonic_samples = sonicReadFloatFromStream(controller->m_audio_speed_convert, (float*)is->audio_buf1, nb_samples);
                    } else if (is->audio_tgt.fmt == AV_SAMPLE_FMT_S16) {
                        sonic_samples = sonicReadShortFromStream(controller->m_audio_speed_convert, (short*)is->audio_buf1, nb_samples);
                    } else {
                        av_log_error("sonic not support fmt:%d", is->audio_tgt.fmt);
                    }
                    /*
                     * mark: 其实可以理解为 audio_buf1 是 audio_buf 的一个备份，相当于每次操作 audio_buf 时，
                     * 都是先将 audio_buf 赋值给 audio_buf1，然后对 audio_buf1 进行处理，处理完成后再赋值给 audio_buf。
                     * 就类似 swap 方法中的 temp
                     * 然后将倍数后得到的数据更新到我们配置中，即 audio_buf、audio_buf_size、audio_buf_index。
                     * 但我感觉 audio_buf_index = 0 不需要写了，因为上面才执行过了
                     */
                    is->audio_buf = is->audio_buf1;
                    is->audio_buf_size = sonic_samples * av_get_bytes_per_sample(is->audio_tgt.fmt) * is->audio_tgt.channels;
                    is->audio_buf_index = 0;
                }
            }

        }
        if (is->audio_buf_size == 0)
            continue;
        // 本次剩余的音频数据大小还有多少
        len1 = is->audio_buf_size - is->audio_buf_index;
        // 剩余数据如果比需要的值（len） 多，那么只需要使用 len 这么多即可
        if (len1 > len)
            len1 = len;
        /*
         * mark：首先这个 if 其实不需要写，这个只是为了优化，因为 SDL 默认就是会播放 buf 的最大声音，即原始数据，所以直接将 buf 原始数据 copy 给 stream 中即可，SDL 就会直接播放这个 stream
         * 但是如果不是最大声音，那么 SDL 就需要处理 stream 数据了，比如修改振幅大小（声音的大小就是振幅的大小），所以需要 SDL_MixAudio 这个方法
         * 这个方法就是将 buf 调整振幅，调整到 audio_volume 大小后，就变成了新的音频数据了，再将这个数据 copy 给 stream 中，然后 SDL 再使用这个 stream 进行播放。
         * 即原理是：SDL 会直接播放这个 stream，给的 stream 是什么就播放什么，如果播放最大声音，那么默认就是 buf，不需要进行振幅调整。如果播放非最大声音，那么就需要进行振幅调整。
         */
        if (is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1);
        else {
            memset(stream, 0, len1);
            if (is->audio_buf)
                SDL_MixAudio(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1, is->audio_volume);
        }
        /*
         * mark：
         * stream 需要的数据本轮读取了 len1 了，所以 len -= len1，stream += len1
         * buf 的数据被读取了 len1 了，所以 is->audio_buf_index += len1
         */
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    if (!std::isnan(is->audio_clock)) {
        /*
        * mark：使用原始媒体时间 audio_clock，不再除以倍率。
        * 音频硬件缓冲区的时长是实时单位，需要乘以倍率转换为媒体时间：
        *   当前播放位置 = audio_clock(媒体时间) - 缓冲区时长(实时) * 倍率
        * 2x时，缓冲区0.05s实时 = 0.1s媒体时间，所以当前听到的是 audio_clock - 0.1
        */
        // todo: audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec 公式为什么就是 pts了
        controller->set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec * controller->get_playback_rate(),
                               is->audio_clock_serial, g_audio_callback_time / 1000000.0);
    }
    // av_log_info("callback video_clk=%f, audio_clk=%f, pts=%f, is->audio_clock=%f, sdl_buf = %f, bytes_per_sec=%d\n", controller->get_clock(&is->vidclk), controller->get_clock(&is->audclk),is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock, (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size), is->audio_tgt.bytes_per_sec);

}

/*
 * opaque: userdata数据，传给sdlcallback 回调函数
 * wanted_channel_layout： 期望使用的频道布局，即解码器得到的频道布局，同样这个也是原始音频默认设置的
 * wanted_nb_channels：期望使用的频道数，即解码器得到的频道数，同样这个也是原始音频默认设置的
 * wanted_sample_rate：期望使用的采样率，即解码器得到的采样率，同样这个也是原始音频默认设置的
 * audio_hw_params：音频硬件参数，期望的参数不一定能使用，所以我们需要得到最终本地能运行的参数
 * 这个方法其实就是就两个作用：
 *  1. 打开 SDL_OpenAudio，并设置回调函数
 *  2. 得到最终本地能运行的音频硬件参数
 */
int VideoCtrl::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels,
                         int wanted_sample_rate, struct AudioParams *audio_hw_params) {
    SDL_AudioSpec wanted_spec, spec;
    const char* env;
    /*
     * mark： next_nb_channels 数组的设计是:用当前通道数作为索引,查表得到下一个要尝试的通道数。
     *  8+声道 → 6声道 → 4声道 → 2声道 → 1声道 → 失败
        7声道  → 6声道 → 4声道 → 2声道 → 1声道 → 失败
        6声道  → 4声道 → 2声道 → 1声道 → 失败
        5声道  → 6声道 → 4声道 → 2声道 → 1声道 → 失败  (5直接跳到6很奇怪)
        4声道  → 2声道 → 1声道 → 失败
        3声道  → 6声道 → 4声道 → 2声道 → 1声道 → 失败  (3跳到6也很奇怪)
        2声道  → 1声道 → 失败
        1声道  → 失败
     */
    // 规范的通道数和采样率
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    // 从规范的标准里面获取数据，从最后一个慢慢往前找
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_nb_channels) - 1;

    env = getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    // 给 wanted_spec 赋值，准备传入 SDL_OpenAudio 了
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    /*
     * mark: SDL的缓冲区大小
     * SDL_AUDIO_MIN_BUFFER_SIZE 是SDL要求的最小缓冲区大小(通常是512或1024)
     * 右边公式是根据我们输入的采样率来得到一个缓冲区大小
     * 这个缓冲区是平衡播放延迟和稳定性的
     * 实际例子:
        假设采样率 44100 Hz:
        每次回调样本数 = 44100 / 15 ≈ 2940
        向上取2的幂 = 4096
        最终 samples = max(512, 4096) = 4096

        缓冲区时长 = 4096 / 44100 ≈ 93ms
    这意味着:
        SDL每93ms调用一次回调函数
        回调函数需要提供4096个样本数据
        音频延迟约93ms(可接受范围)
    如果不设置会怎样?
        设为0或太小: 音频卡顿、爆音、断断续续
        设为太大: 音画不同步,声音明显滞后于画面
        不设置为2的幂: SDL可能拒绝打开音频设备,或性能下降
     */
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log_error("Invalid sample rate or channel count");
        return -1;
    }

    /*
     * mark:找到比期望采样率低一档的标准采样率。
     * 当然也可以找到当前采样率的下标，只是下面就不能用降级的时候就不能用 next_sample_rate_idx--，而是需要使用 --next_sample_rate_idx 了。
     */
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
#if defined(_WIN32)
    /*
     * mark：为了修复 win 下 XAudio2: XAudio2Create() failed at open. 的这个报错。
     * 在Windows系统中，音频设备和音频处理往往依赖于COM架构，所以我们需要在  SDL_OpenAudio 前初始化 COM，即调用 CoInitialize(NULL);
     */
    CoInitialize(NULL);
#endif
    /*
     * mark：这里是判断，如果当前配置无法打开SDL，那么就尝试降级【即修改通道数和采样率】
     * 其实好多这种异常情况根本不会发生，写这么多代码就是为了防止很小部分可能会发生的错误
     */
    while (!m_audio_output.open(wanted_spec, &spec)) {
        av_log_warning("SDL_OpenAudio (%d channel, %d Hz): %s\n", wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        // 声道降级，尝试使用低标准的声道看看能不能打开audio
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        // 如果声道降级后全部尝试都不行了，那么开始尝试采样率降级，此时声道就使用默认的声道
        if (!wanted_spec.channels) {
            // 采样率降级，尝试使用低标准的采样率看看能不能打开audio
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log_error("no more combinations to try, audio open failed\n");
                return -1;
            }
        }
        // 声道降级后，需要重新获取该声道对应的频道布局
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    /*
     * mark：如果 SDL 推荐的声道数和期望的声道数不一致，那么就需要重新获取该声道数对应的频道布局
     * 而如果本地没有这个新的声道数对应的频道布局，那么SDL也无法播放，那么就返回 -1
     */
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            av_log_error("SDL advised channels layout %d is not support\n", spec.channels);
            return -1;
        }
    }

    // 给 audio_hw_params 设置我们本次的音频硬件参数最终使用的值了
    switch (spec.format) {
    case AUDIO_U8:
        audio_hw_params->fmt = AV_SAMPLE_FMT_U8;
        break;
    case AUDIO_S16LSB:
    case AUDIO_S16MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
        break;
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_S32;
        break;
    case AUDIO_F32LSB:
    case AUDIO_F32MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_FLT;
        break;
    default:
        audio_hw_params->fmt = AV_SAMPLE_FMT_U8;
        break;
    }
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels = spec.channels;
    // 获取1个样本的字节大小
    audio_hw_params->frame_size = av_samples_get_buffer_size(nullptr,audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    // 获取1秒的音频数据字节数
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(nullptr, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);

    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log_error("av_samples_get_buffer_size failed\n");
        return -1;
    }
    /*
     * mark：这里返回的是 SDL 的音频缓冲区大小，这个大小是 SDL 自己算的，
     * 但其实内部公式如下：size = samples × channels × bytes_per_sample
     * 其实就是一个音频帧的大小
     */
    return spec.size;
}

/*
 * 其实这个方法不应该叫解码帧，因为解码帧是在 audio_thread 里面解码的，
 * 实际上这个方法的作用是：读取帧的数据，然后进行重采样后写入 buf 指针提供给callback 使用，
 * 而视频的 seek 和 暂停也是在这里实现的，即不写入 buf 数据，那么就是暂停，buf 写入的是 seek 后的 数据，那么就是seek功能
 */
int VideoCtrl::audio_decode_frame(VideoState *is) {
    int data_size, resampled_data_size;
    int64_t wanted_channel_layout;
    int wanted_nb_samples;
    Frame *af;

    // 根据 m_audio_mode 决定从哪个帧队列读取

    FrameQueue *src_sampq =  &is->sampq;
    PacketQueue *src_audioq =  &is->audioq;

    /*
     * mark：这里就是音频暂停时候的处理，此时返回-1，则外面就获取不到播放的音频数据了
     * 注意，暂停并不是说马上停止什么都不干，暂停指的是当前帧播放完成后，不播放下一帧了，
     * 故这里虽然返回-1了，外面还有当前帧的缓存数据，所以此刻外面还在播放，当当前帧播放完成了后才会没有数据播放，因为这里 return -1 了嘛，读取不到下一帧了
     */
    if (is->paused && m_settings.forcePlay())
        return -1;
    do {
#if defined(_WIN32)
        /*
         * mark：当音频帧队列里面没有数据可以读取的时候，那么每1毫秒检测判断一次是否有数据，直到时间超过音频硬件缓冲区大小的一半数据【转换后的时间】还没有读取到，那么就返回-1
         */
        while (frame_queue_nb_remaining(src_sampq) == 0) {
            if ((av_gettime_relative() - g_audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2) {
                return -1;
            }
            av_usleep(1000);
        }
#endif \
    // 这里是死等，直到报错或者有数据
        if (!((af = frame_queue_peek_readable(src_sampq))))
            return -1;
        frame_queue_next(src_sampq);
    }while (af->serial != src_audioq->serial);
    // 计算当前帧的字节大小
    data_size = av_samples_get_buffer_size(nullptr, av_frame_get_channels(af->frame), af->frame->nb_samples, (AVSampleFormat)af->frame->format, 1);
    /*
     * 判断当前帧的声道类型，如果当前帧自带的属性声道数和声道类型匹配不上，那么我们就使用这个声道数的默认声道类型
     */
    wanted_channel_layout =
        (af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
            af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);
    /*
     * (wanted_nb_samples != af->frame->nb_samples && !is->owned_swr_ctx) 这个判断又是为了什么？
     * 如果完全相同，是不需要创建重采样器的
     * mark: 重采样器 是根据音频源参数和目标参数进行转换的，如果此时参数变了，那么就需要重新初始化 重采样器
     */
    // 根据是否使用 stem 源来选择重采样器和参数
    SwrContextPtr &swr_ctx = is->owned_swr_ctx;
    AudioParams *p_audio_src = &is->audio_src;
    AudioParams &audio_tgt = is->audio_tgt;

    if (af->frame->format != p_audio_src->fmt ||
        wanted_channel_layout != p_audio_src->channel_layout ||
        af->frame->sample_rate != p_audio_src->freq ||
        (wanted_nb_samples != af->frame->nb_samples && !swr_ctx)) {
        /*
         * mark：哦知道了，audio_src 保存的是当前音频帧的参数，而 audio_tgt 保存的是SDL播放的参数（或者说当前机器输出声音的参数）
         */
        swr_ctx.reset(swr_alloc_set_opts(nullptr,
                                        audio_tgt.channel_layout, audio_tgt.fmt, audio_tgt.freq,
                                        wanted_channel_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
                                        0, nullptr));
        if (!swr_ctx || swr_init(swr_ctx.get()) < 0) {
            av_log_error( "cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels\n",
                         af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), av_frame_get_channels(af->frame),
                         audio_tgt.freq, av_get_sample_fmt_name(audio_tgt.fmt), audio_tgt.channels);
            swr_ctx.reset();
            return -1;
        }
        // 更新当前音频帧的参数
        p_audio_src->channel_layout = wanted_channel_layout;
        p_audio_src->channels = av_frame_get_channels(af->frame);
        p_audio_src->freq = af->frame->sample_rate;
        p_audio_src->fmt = (AVSampleFormat)af->frame->format;
    }
    // 如果我们设置了音频重采样器，说明需要进行音频重采样
    if (swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        /*
         * mark：计算当前样本点数量重采样后的样本数量,即 af->frame->nb_samples 重采样后的样本数点数
         * 假设 原始采样率：44100， 样本数 1024
         * 目标采样率：48000， 那么转换后得到的样本数量是 1024 * 44100 / 48000 = 896
         * 因为单纯重采样转换后的播放时长是不会变化的，当前样本数占用当前采样率的比例应该跟之前一样。
         * +256 是为了防止创建的 buf1 太小了，无法获取完重采样后得到的所有数据，也可以不写 +256，或者改成 +512、+1024 都可以。
         */
        int out_count = (int64_t)wanted_nb_samples * audio_tgt.freq / af->frame->sample_rate + 256;
        // 计算这么多个样本点的字节大小
        int out_size = av_samples_get_buffer_size(nullptr, audio_tgt.channels, out_count, audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log_error("av_samples_get_buffer_size failed\n");
            return -1;
        }
        // 如果音频同步后，采样点数量需要改变，那么让 重采样器帮我们补偿（多了就填充空白样本点，少了就删除几个样本点）
        if (wanted_nb_samples != af->frame->nb_samples) {
            /*
             * (wanted_nb_samples - af->frame->nb_samples) * audio_tgt.freq / af->frame->sample_rate 是重采样后要补充多少个样本点
             * wanted_nb_samples * audio_tgt.freq / af->frame->sample_rate 是 af->frame->nb_samples 重采样后的样本数点数
             */
            if (swr_set_compensation(swr_ctx.get(), (wanted_nb_samples - af->frame->nb_samples) * audio_tgt.freq / af->frame->sample_rate,
                                     wanted_nb_samples * audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log_error("swr_set_compensation failed\n");
                return -1;
            }
        }
        /*
         * mark:给 audio_buf1 分配至少 out_size 这么多个字节，返回 is->audio_buf1_size 为实际分配的字节数
         * buf1 是我们逻辑处理时的变量，实际上逻辑处理完了之后，就会把 buf1 赋值给 buf，然后外面的回调使用的就是 buf
         */
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        // 进行重采样，out_count 并不是说真的要还给我们这么多个样本点，而是说 out 的最大值是 out_count
        len2 = swr_convert(swr_ctx.get(), out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log_error("swr_convert failed\n");
            return -1;
        }
        /*
         * mark：这里就能看出来，假设我们的 out_count 没有 +256，那么返回的数据可能是比 out_count 更多的，所以我们需要给 out_count 多加一点，适配这种情况
         * 即本来 out_count 是 1040 个样本点数量，而重采样后可能会多给一点，1048 或者 1052 之类的都有可能，这个是重采样器内部的处理逻辑，我们不需要懂，我们只需要知道有这么情况就行了，
         * 所以我们给 out_count + 256，就算你重采样后多返回了几个字节，我们创建的 buf1 也能全部接受，不会出现数据丢失的情况。
         */
        if (len2 == out_count) {
            av_log_warning("audio_buffer is probably too small\n");
            if (swr_init(swr_ctx.get()) < 0)
                swr_ctx.reset();
        }
        is->audio_buf = is->audio_buf1;
        // 本次重采样后的样本点总的字节大小
        resampled_data_size = len2 * audio_tgt.channels * av_get_bytes_per_sample(audio_tgt.fmt);
        // 如果不需要重采样，则直接使用原始的音频数据，数据字节大小也就是当前音频帧的字节大小
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }
    if (!std::isnan(af->pts)) {
        /*
         * mark：这里只需要计算正常播放速度的pts值即可，倍数相关的处理在外面进行
         */
        is->audio_clock = af->pts + (double)af->frame->nb_samples / (double)af->frame->sample_rate;
        // av_log_info("aduio_decode_frame is->audio_clock=%f, af->pts=%f\n",is->audio_clock, af->pts);
    }
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;
    m_settings.setForcePlay(true); //强制播放一帧事件完成

    return resampled_data_size;
}

// 做音视频同步，像视频一样，时钟没对上就加快播放或者延迟播放，音频则是快了则减少样本点，慢了则增加样本点
int VideoCtrl::synchronize_audio(VideoState *is, int nb_samples) {
    // 默认情况下不需要追赶，则该播放多少样本点，就播放多少样本点
    int wanted_nb_samples = nb_samples;
    // 如果不是音频为主时钟，则需要进行同步操作了
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;
        // 计算当前音频需要同步时间（媒体时间单位），然后转换为实时单位
        diff = (get_clock(&is->audclk) - get_master_clock(is)) / m_settings.rate();
        // 只有在适当的时间差下，才进行同步，差太多就默认为没问题，是故意这样的，就不需要进行同步
        if (!std::isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            /*
             * mark：这些公式是为了避免音频频繁跳变，这里做了一个平滑处理，
             * 视频你可以每一张图片都多播放一毫秒或者慢放一毫秒，这个无所谓感觉不出来，
             * 但是声音如果频繁来回变快或者慢，人耳是能感受出来的，所以我们决定在某个范围内下不做同步，超过这个范围再做同步
             */
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                is->audio_diff_avg_count++;
            }else {
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
                // is->audio_diff_threshold 就是我们打开SDL的时候设置的SDL缓冲区大小，即一般是一帧的音频对应的时间
                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    /*
                     * 如果我们快了，则 diff 是大于 0 的，所以我们需要填充多一点样本来播放，
                     * 即本来这一帧播放 1024个样本点，那么我们就播放 1030 个样本点，多的这个6个样本点是等会 swr 填充的空白数据给SDL播放的
                     * 如果慢了则同理
                     */
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    // 当然我们不能没有限制的同步，我们每次同步的时候只能在多加一点点样本数，这样人耳容易接受
                    min_nb_samples = nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100;
                    max_nb_samples = nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100;
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
                av_log_trace("diff=%f, adiff=%f, sample_diff=%d, apts=%0.3f, diff_threshold=%f\n", diff, avg_diff, wanted_nb_samples - nb_samples, is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }
    return wanted_nb_samples;
}

// 其实这个方法可以直接使用 is->av_sync_type 替代，但是为了健壮性，即如果 is->av_sync_type 设置了 视频，但是没有视频流，为了避免这种情况，这个方法的健壮性就体现出来了
int VideoCtrl::get_master_sync_type(VideoState *is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_VIDEO_MASTER;
    } else {
        return AV_SYNC_AUDIO_MASTER;
    }
}

// 解码音频包线程，单纯读取音频包然后解析为音频帧写入帧队列
int VideoCtrl::audio_thread(void *arg) {
    VideoState *is = (VideoState*)arg;
    AVFrame *frame = av_frame_alloc();
    Frame *af;
    int got_frame = 0;
    AVRational tb;
    /*
     * mark: 这个 ret 现在目前没用，它用来判断音频滤镜相关的错误，现在暂时没用，所以直接返回 0
     */
    int ret = 0;
    if (!frame)
        return AVERROR(ENOMEM);

    do {
        if ((got_frame = decoder_decode_frame(&is->auddec, frame, nullptr)) < 0)
            goto the_end;
        if (got_frame) {
            // 时间基 = 频率的倒数，即 1 / 441000
            tb = {1, frame->sample_rate};
            if (!((af = frame_queue_peek_writable(&is->sampq))))
                goto the_end;
            // 转换为现实时间播放秒数
            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            // av_log_info("audio_thread frame->pts=%lld\n", frame->pts);
            af->pos = av_frame_get_pkt_pos(frame);
            af->serial = is->auddec.pkt_serial;
            af->duration = av_q2d({frame->nb_samples, frame->sample_rate});

            av_frame_move_ref(af->frame, frame);
            frame_queue_push(&is->sampq);
        }
        // 目前来说这里可以写 while (true)
    }while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
the_end:
    ;  // 空语句，兼容 C++23 之前的标准
    av_frame_free(&frame);
    return ret;
}

// 这里的解码线程--先执行解码，在执行读取数据，是为了防止延迟，如果先读取数据，再解码，就会有延迟，因为我读取了第一个图片后，你才开始启动解码线程，等你启动完毕后，我现在都在读取第三个图片了。而如果先启动解码线程，此时我一直在解码中，你来一个图片我就解码一个图片，没有丝毫延迟了。
int VideoCtrl::video_thread(void *arg) {
    VideoState *is = (VideoState *) arg;
    AVFrame *frame = av_frame_alloc();
    AVFrame *filteredFrame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    // tb 是 pts 的时间基，例如 1/90000, frame_rate 是视频帧率的时间基，例如 1/25
    AVRational tb = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);
    if (!frame || !filteredFrame) {
        av_frame_free(&frame);
        av_frame_free(&filteredFrame);
        return AVERROR(ENOMEM);
    }

    while (true) {

        // 从解码器中获取数据【或者说包队列中】
        ret = get_video_frame(is, frame);
        if (ret < 0)
            goto out;
        if (!ret)
            continue;

        /*
         * 可选滤镜链：仅在配置了滤镜描述时启用（默认关闭，走与原来完全一致的路径）。
         * 滤镜图按首个解码帧的格式惰性构建，一次 push 后把滤镜产出的所有帧依次入队。
         */
        if (!is->filter_description.empty()) {
            if (!is->video_filter.isActive()
                && !is->video_filter.create(is->filter_description.c_str(), frame, tb)) {
                av_log_error("video filter chain unavailable, disabling it\n");
                is->filter_description.clear();
            }
        }

        if (is->video_filter.isActive()) {
            const AVRational filterTimeBase = is->video_filter.outputTimeBase();
            if (is->video_filter.push(frame) < 0) {
                av_log_error("cannot push frame into video filter chain\n");
                goto out;
            }
            av_frame_unref(frame);
            while ((ret = is->video_filter.pull(filteredFrame)) >= 0) {
                duration = (frame_rate.num && frame_rate.den ? av_q2d({frame_rate.den, frame_rate.num}) : 0.0);
                pts = (filteredFrame->pts == AV_NOPTS_VALUE) ? NAN : filteredFrame->pts * av_q2d(filterTimeBase);
                ret = queue_picture(is, filteredFrame, pts, duration,
                                    av_frame_get_pkt_pos(filteredFrame), is->viddec.pkt_serial);
                av_frame_unref(filteredFrame);
                if (ret < 0)
                    goto out;
            }
            continue;
        }

        // 正常来说应该有帧率的，即 1/25，那么得到的 duration 就是 40 ms，但是如果没有，那么我们只能标记为0代表有问题
        duration = (frame_rate.num && frame_rate.den ? av_q2d({frame_rate.den, frame_rate.num}) : 0.0);
        // 这个 pts 是计算真实时间播放的时长，单位秒
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        /*
         * mark：ffmpeg8.0+ pos 现在不能从 av_frame_get_pkt_pos 获取了，现在这个方法没了，但是可以通过 pkt.pos 获取
         */
        ret = queue_picture(is, frame, pts, duration, av_frame_get_pkt_pos(frame), is->viddec.pkt_serial);
        av_frame_unref(frame);
        if (ret < 0)
            goto out;

    }
out:
    av_frame_free(&frame);
    av_frame_free(&filteredFrame);

    return 0;
}

int VideoCtrl::get_video_frame(VideoState *is, AVFrame *frame) {
    int got_picture;
    if ( (got_picture = decoder_decode_frame(&is->viddec, frame, NULL)) < 0)
        return -1;

    /*
     * 硬件解码输出的帧只在显存中，后续队列和 SDL 纹理上传都按软件帧处理，
     * 因此在这里统一转换为软件帧，使渲染路径无需感知硬件解码。
     */
    if (got_picture && is->hw_device.isActive() && frame->format == is->hw_device.pixelFormat()) {
        AVFrame *softwareFrame = av_frame_alloc();
        if (!softwareFrame || !is->hw_device.toSoftwareFrame(frame, softwareFrame)) {
            av_log_error("cannot transfer hardware frame to system memory\n");
            av_frame_free(&softwareFrame);
            av_frame_unref(frame);
            return -1;
        }
        av_frame_unref(frame);
        av_frame_move_ref(frame, softwareFrame);
        av_frame_free(&softwareFrame);
    }

    if (got_picture) {
        // 现实的时间播放时长
        double dpts = NAN;
        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;
        /*
         * mark：自动推断宽高比。虽然 avcodec_receive_frame 拿到的这个帧，已经有宽高比了。但是 av_guess_sample_aspect_ratio 拿到的宽高比一定是最准确的，所以再来获取一次
         */
        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);
        if (framedrop > 0) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diif = dpts - get_master_clock(is);
                if (!std::isnan(diif) && fabs(diif) < AV_NOSYNC_THRESHOLD && diif - is->frame_last_filter_delay < 0 && is->viddec.pkt_serial == is->vidclk.serial && is->videoq.nb_packets) {
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}

int VideoCtrl::queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial) {
    Frame *vp;
    if (!(vp = frame_queue_peek_writable(&is->pictq)))
        return -1;
    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    av_frame_move_ref(vp->frame, src_frame);
    frame_queue_push(&is->pictq);
    return 0;
}

void VideoCtrl::do_exit(VideoState *is) {
    if (is) {
        stream_close(is);
        m_cur_stream = nullptr;
    }

    // 重新创建渲染器和纹理--因为渲染器一定会失效，所以我们直接创建一个新的
    if (m_video_output_resources.window()) {
        if (m_video_output_resources.recreateRenderer()) {
            // 用 m_last_frame 重新创建纹理（av_frame_ref 保证了帧数据在 stream_close 后仍然有效）
            // 用户主动停止时不保留最后一帧，清空画面
            if (!m_user_stop && m_last_frame && m_last_frame->width > 0 && m_last_frame->height > 0) {
                av_log_info("do_exit: recreating texture from m_last_frame, size=%d x %d, format=%d\n",
                            m_last_frame->width, m_last_frame->height, m_last_frame->format);
                int sdlPixFmt = m_last_frame->format == AV_PIX_FMT_YUV420P ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888;
                if (m_video_output_resources.ensureTexture(sdlPixFmt, m_last_frame->width, m_last_frame->height,
                                                          SDL_BLENDMODE_NONE)) {
                    SwsContextPtr sws_ctx;
                    upload_texture(m_video_output_resources.texture(), m_last_frame.get(), sws_ctx);
                    av_log_info("do_exit: texture recreated successfully\n");
                }
            } else if (m_user_stop) {
                // 用户主动停止：清空画面，释放最后一帧引用
                av_log_info("do_exit: user stop, clearing display\n");
                m_last_frame.reset();
                m_frame_width = 0;
                m_frame_height = 0;
                // 清空渲染器为黑色
                m_video_output_resources.beginFrame();
                m_video_output_resources.endFrame();
            } else {
                av_log_info("do_exit: no m_last_frame to recreate texture, m_last_frame=%p\n", (void*)m_last_frame.get());
            }
        }
    }

    // 这里不销毁窗口，因为窗口我们需要一直渲染
    m_video_open = false; // 标记视频窗口已关闭--其实是播放状态的关闭，窗口实际还在使用
    // 切换文件时旧会话的结束事件属于过期事件，不投递给播放服务
    if (!isCurrentSession(is)) {
        return;
    }
    if (m_user_stop) {
        emit SigUserStopFinished();
    } else {
        emit SigStopFinished();
    }
}

void VideoCtrl::stream_close(VideoState *is) {
    is->abort_request = 1;
    if (is->read_tid.joinable()) {
        is->read_tid.join();
    }

    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->audio_stream >= 0)
        stream_component_close(is, is->audio_stream);

    is->demux_reader.close();
    is->owned_ic.reset();
    is->ic = nullptr;

    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);

    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);

    is->owned_continue_read_thread.reset();

    delete is;

    // 关闭 stem 资源
    //CloseStemSource();
}

void VideoCtrl::stream_component_close(VideoState *is, int stream_index) {
    AVFormatContext *ic = is->ic;
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;

    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        decoder_abort(&is->auddec, &is->sampq);
        m_audio_output.close();
        decoder_destroy(&is->auddec);
        is->owned_swr_ctx.reset();
        av_freep(&is->audio_buf1);
        is->audio_buf1_size = 0;
        is->audio_buf = nullptr;
        /*
             * mark：切换音频流时，需要销毁 Sonic 对象，因为新流的参数可能不同，比如采样率，通道数
             */
        if (m_audio_speed_convert) {
            sonicDestroyStream(m_audio_speed_convert);
            m_audio_speed_convert = nullptr;
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        decoder_abort(&is->viddec, &is->pictq);
        decoder_destroy(&is->viddec);
        // 解码器上下文释放后再释放硬件设备，避免 avctx->opaque 悬空
        is->hw_device.reset();
        break;
    default:
        break;
    }

    /*
     * mark：这个流不使用了，那么就可以置空了，即标记为丢弃，降低CPU使用，提高效率
     */
    ic->streams[stream_index]->discard = AVDISCARD_ALL;

    /*
     * mark：为什么不能和上面的if放在一起，或者说能不能放在一起。
     *  上面的 if 是为了实现删除解码器，而解码器还在操作流，所以 ic->streams[stream_index]->discard = AVDISCARD_ALL; 必须放在它的后面。
     *  而当解码器停用后，流已经停止使用，那么就可以标记流被丢弃了，然后我们在这个if里面才能更新我们的视频相关的信息。
     */
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = nullptr;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = nullptr;
        is->video_stream = -1;
        break;
    default:
        break;
    }
}

void VideoCtrl::loop_thread(VideoState *curStream) {
    SDL_Event event;
    double incr, pos, frac;

    m_play_loop = true;
    m_idle_loop = true;  // 在线程启动时设置，若 start_play 已将其置为 false 则空闲循环不会进入
    while (m_play_loop) {
        double x;
        refresh_loop_wait_event(curStream, &event);
        // 处理键盘事件
        switch (event.type) {
        case SDL_WINDOWEVENT:
            // 窗口大小改变事件
            switch (event.window.event) {
            // 同步窗口大小改变
            case SDL_WINDOWEVENT_RESIZED:
                m_screen_width = m_cur_stream->width = event.window.data1;
                m_screen_height = m_cur_stream->height = event.window.data2;
                // 注意，这里没有break，也就是窗口大小改变的时候也会设置  m_cur_stream->force_refresh = 1;
                /*
                         * mark: keep_last
                         * 可以想一想，如果没有 keep_last，那么每一帧图片播放完了就丢了。那么现在就出现了一个问题，当暂停的时候，当前帧已经播放过了，所以已经丢了。
                         * 而窗口大小此时拖动了，我们需要同步 rescale 图片的大小，但此时图片没了，那不就不能同步图片的大小改动了吗？那么此时我们又需要根据这个需求，创建一个全局变量frame，自己每次保存一下当前图片。
                         * 然后等到暂停的时候，把这个图片拿来做 rescale 处理。这样的处理太麻烦了，而 keep_last 直接就生效了，这就是为什么视频队列一定要配置 keep_last 参数，否则这个功能就没了。
                         */
            // 当窗口从遮挡变成显示的时候（比如最小化了，或者被其他前置页面挡住了），强制刷新【防止出现卡顿，加载的第一时刻没有图片刷新——但是好像没出现这种情况】
            case SDL_WINDOWEVENT_EXPOSED:
                m_cur_stream->force_refresh = 1;
            default:
                break;
            }
            break;
        // 退出
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            do_exit(m_cur_stream);
            break;
        default:
            break;
        }
    }

    do_exit(curStream);

    /*
     * 空闲事件循环：播放结束后继续保持 SDL 事件循环，
     * 直接监听 SDL 自己的窗口大小改变事件进行重绘，无需 Qt 信号通知。
     * 因为 D3D 渲染器只能在创建它的线程上使用，
     * 所以必须在 loop_thread（即 SDL 线程）上进行渲染，不能从 Qt 主线程直接渲染。
     *
     * 注意：m_idle_loop 在线程启动时已设为 true。
     * 若 start_play 在播放循环期间将其置为 false，此处 while 条件为 false，
     * 线程直接退出，避免 join() 死锁。
     */
    while (m_idle_loop) {
        applyPendingRenderTarget();
        // 渲染目标切换后窗口已被释放，用最后一帧在新窗口上重绘
        if (!m_video_output_resources.window()) {
            redrawLastFrame();
        }
        // SDL_WaitEventTimeout 返回 0 表示超时无事件，此时 event 未填充，需跳过处理
        if (!SDL_WaitEventTimeout(&event, 100))
            continue;
        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                m_screen_width = event.window.data1;
                m_screen_height = event.window.data2;
                // fall through，resize 和 exposed 都需要重绘
            case SDL_WINDOWEVENT_EXPOSED:
                if (m_video_output_resources.isReady() && g_show_rect_mutex.tryLock()) {
                    m_video_output_resources.beginFrame();
                    if (m_video_output_resources.texture() && m_frame_width > 0 && m_frame_height > 0) {
                        SDL_Rect rect;
                        calculate_display_rect(&rect, 0, 0, m_screen_width, m_screen_height,
                                               m_frame_width, m_frame_height, m_frame_sar);
                        m_video_output_resources.presentTexture(rect, m_frame_flip_v);
                    }
                    m_video_output_resources.endFrame();
                    g_show_rect_mutex.unlock();
                }
                break;
            default:
                break;
            }
            break;
        case SDL_QUIT:
            m_idle_loop = false;
            break;
        default:
            break;
        }
    }
}

void VideoCtrl::refresh_loop_wait_event(VideoState *is, SDL_Event *event) {
    double remainingTime = 0.0;
    SDL_PumpEvents();

    // 当没有键盘事件的时候，就进行图片刷新。也就是说一旦遇到的了键盘事件，那么就退出了这个while循环，返回到上一层的switch处理对应的键盘事件了
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) && m_play_loop) {
        // 渲染目标切换请求由 SDL 线程执行，避免跨线程操作渲染资源
        applyPendingRenderTarget();
        // 如果需要休眠等待的话
        if (remainingTime > 0.0)
            av_usleep((int64_t)(remainingTime * 1000000.0));
        remainingTime = REFRESH_RATE;
        /*
         * mark: 这个 if (!is->paused || is->force_refresh)  其实是一个优化，即暂停状态下不需要刷新图片，那么不需要进入 video_refresh 方法，不用浪费 CPU 资源
         * 举例：
         * 1. 正常播放下，!is->paused 为 true，那么进入 video_refresh 方法，进行图片刷新。
         * 2. 正常暂停情况下，!is->paused 为 false，is->force_refresh 为 false，那么不进入 video_refresh 方法，减少资源消耗
         * 3. 暂停情况下，图片大小改变了，那么我们需要刷新窗口的图片大小， is->force_refresh 为 true，那么进入 video_refresh 方法，进行图片刷新
         */
        if (!is->paused || is->force_refresh)
            video_refresh(is, &remainingTime);
        SDL_PumpEvents();
    }
}

void VideoCtrl::video_refresh(void *arg, double *remainingTime) {
    VideoState *is = (VideoState*) arg;
    double time;

    if (is->video_st) {
    retry:
        if (frame_queue_nb_remaining(&is->pictq) == 0) {
            // nothing to do
        } else {
            double last_duration, duration, delay;
            Frame *vp, *lastvp;

            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            // 取下一个包
            if (vp->serial != is->videoq.serial) {
                frame_queue_next(&is->pictq);
                goto retry;
            }

            // 当前画面和上一个画面不等，说明是seek，则 frame_timer 不应该是 上一次的 frame_timer + duration，而应该直接取当前时间
            if (lastvp->serial != vp->serial)
                // frametimer 记录的是当前帧的播放时刻；同时 is->frame_timer 也是是时间基，即当前画面播放时间轴。等会需要拿去和真实世界的时间轴做对比
                is->frame_timer = av_gettime_relative() / 1000000.0;
            if (is->paused)
                goto display;
            // 计算上一帧的图片的应该播放总时长
            last_duration = vp_duration(is, lastvp, vp);

            /*
             * mark：进行音视频同步后，上一帧的播放时长，看看是否需要延长或者缩短，即这里得到的是剩余播放时长。这里最终获得的值和 last_duration 差不了多少，为什么差不了多少可见 max_frame_duration 的赋值注释中有说明
             */
            delay = compute_target_delay(last_duration, is);
            time = av_gettime_relative() / 1000000.0;
            // printf("time:%f, is->frame_timer + delay:%f, is->frame_timer:%f, delay:%f,\n", time, is->frame_timer + delay, is->frame_timer, delay);
            // printf("m_screen_height:%d, m_screen_width:%d\n", m_screen_height, m_screen_width);
            /*
             * mark：对于此刻来说，由于 is->frame_timer 的值还没更新过，那么此时 is->frame_timer 的值记录的就是上一帧的播放时刻
             * 这个if的功能：如果此时还没到当前帧的播放时间，那么睡眠等待这段时间，然后播放
             */
            if (time < is->frame_timer + delay) {
                /*
                 * mark：这里没有sleep，为什么就 goto display;了
                 * 因为这里没有设置  is->force_refresh = 1; 就算  goto display 也不会显示。然后结束这个方法，又回到外面的 while了，那个时候 remainingTime 就生效了，就开始 sleep了。
                 * sleep 结束之后回来，就能发现这一帧可以直接播放了，现在这里的if就不会进了，而是继续往下走，显示图片。
                 */
                *remainingTime = FFMIN(is->frame_timer + delay - time, *remainingTime);
                goto display;
            }

            /*
             * mark：能到这里就是代表已经到了这一帧的播放时间了，如果没到的话上面的if已经跳转了
             * 假设：第一帧是 0.1s 播放，第二帧是 0.2s 播放，
             * 而刚刚正在播放第一帧，现在需要播放第二帧了，现在的 time 是0.4，frametimer 是 0.1，delay 是 0.1
             * 那么按道理，第二帧的 frametimer 就应该是 0.2，即 frametimer + delay = 0.1 + 0.1 = 0.2
             * 不对，如果没有调用 display，那 frametimer 不应该更新吧？
             *      答：不，只要能到这里，就代表这个图片肯定被用了，不管是播放还是丢弃，反正已经算是用过了，所以需要更新 frametimer，即当前帧的 frametimer = 上一个 frametimer + delay
             */
            is->frame_timer += delay;
            /*
             * mark：这个if的作用：当差距过大时，不再尝试追赶，而是接受当前状态，以当前时刻为起点重新开始 。这比疯狂丢帧追赶更稳定，用户体验更好。
             * 举个例子：如果 is-frametimer 慢了 time 1s，假设一个图片40ms，那么下面的丢帧逻辑就会连续触发，会连续丢帧，而且是连续丢1000/40=25帧【这样其实会导致图片卡顿，用户体验不好】
             * 那么再极端一点，如果慢了100s呢，那会连续丢帧多少了？那就太离谱了，所以我们认为差距过大，就停止丢帧追赶这个操作。认为当前音视频同步是没问题的，就按照当前的节奏来，当更新了 is->frame_timer = time; 之后，
             * 下一帧图片的 time < is->frame_timer + delay 就是正常节奏的判断，因为 delay 就是前后两帧的时长，而 is->frame_timer 由于刚刚更新过了和 time 并没有 100 的差值了，而是正常的差值了。
             * 此时其实就恢复到了正常节奏了。
             * 【当然，例子里的时间不准确，实际上我们这里经验值认为，超过 0.1s 就算差距过大，即连续丢三帧就会影响用户了，我们这里就不追赶的。当然这个值可以手动改】
             */
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            SDL_LockMutex(is->pictq.mutex);
            if (!std::isnan(vp->pts))
                /*
                 * mark：能走到这代表这一帧图片肯定要显示了，不管是播放还是丢弃，我们都会把他当做已经显示了，所以需要更新此刻的时钟了。
                 */
                update_video_pts(is, vp->pts, vp->pos, vp->serial);
            SDL_UnlockMutex(is->pictq.mutex);

            // 如果还有帧，那么就丢弃当前帧，因为上面判断能发现已经超过当前帧的播放时间了
            if (frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame *nextVp = frame_queue_peek_next(&is->pictq);
                duration = vp_duration(is, vp, nextVp);
                /*
                 * mark：在非逐帧的情况下【!is->step】，且开启了丢帧【framedrop > 0】，如果到了下一帧的播放时间了【time > is->frame_timer +duration】，那么还要这一帧干什么，这一帧就丢帧了，直接进行下一帧的播放判断
                 * 为什么非逐帧才丢帧？因为逐帧模式下，用户就是想看每一帧，丢帧违背了用户的意图。
                 */
                if (!is->step && framedrop > 0 && time > is->frame_timer + duration) {
                    is->frame_drops_late++;
                    frame_queue_next(&is->pictq);
                    goto retry;
                }
            }
            // 能走到这里代表肯定是选择播放当前帧了，那么更新队列的值
            frame_queue_next(&is->pictq);
            is->force_refresh = 1;
            /*
             * mark：这里就是逐帧功能的最终实现，举例：
             * 1. 正常播放情况下，is->step 为 0，不会进入这个 if 执行暂停播放
             * 2. 正常暂停情况下，is->step 为 0，不会进入这个 if 执行暂停播放
             * 3.1 正常播放情况下按下逐帧功能，is->step 为 1，!is->paused 为 true，进入 if 暂停播放【满足逐帧只播放一帧】
             * 3.2 继续按下逐帧功能，此时处于暂停状态，但是按下逐帧的时候会判断是否处于 is->paused 【可见 step_to_next_frame 方法】,处于则开启播放状态，则又和3.1的流程一样了
             * 4.1 正常暂停情况下按下逐帧功能，此时 is->paused 为true，但是按下逐帧的时候会判断是否处于 is->paused 【可见 step_to_next_frame 方法】,
             * 处于则开启播放状态，则又和3.1的流程一样了
             * 4.2 继续按下逐帧功能，其实和4.1情况是一样的
             */
            if (is->step && !is->paused)
                stream_toggle_pause(is);

        }
    display:
        /*
         * mark: keep_last
         * 首先我们可以发现，如果 is->pictq.rindex_shown 为 0，那么 video_display 方法将永远不会执行，那么图片将永远不会显示。这里也表示了对于图片来说，keep_last 是一定要开启的
         * 然后说一下这里为什么要加 is->pictq.rindex_shown 这个判断。
         * 1. 假设事件循环先执行到这里，而解码线程还没开始运行，即frame队列里面一张图片都没有，那么此时没有图片，我们肯定不能显示，那我们是不是这里判断得换成 is->pictq->size > 0 才行
         * 2. 假设有一段时间内没有视频包，比如 0-1min有视频，画面正常显示，1-2min没有视频，只有音频【即进入上面的if分支 if (frame_queue_nb_remaining(&is->pictq) == 0)】。那么我们肯定不愿意黑屏显示吧，而是想要一直显示最后一张图片。
         *      那么我们此时是不是就得判断，队列里面得有图片（或者说显示过图片了，因为 keep_last 会保存图片），然后我们才能取最后一张图片显示。【这个判断其实就是 is->pictq.rindex_shown 本身，或者也可以想一想有没有其他逻辑可以实现这个判断】
         * 对于上面两点要求来说：其实就一个判断，如果从来没有过图片，那么不能播放。如果有过图片，那么就能播放【不管队列里面现在还有没有图片，如果没有图片，因为之前有过图片了，所以我们缓存了最后一帧图片可以随时使用。如果队列有图片，那更简单，直接播放这个图片就行了】。
         * 对于 is->pictq.rindex_shown 来说，
         * 如果还没有过图片，那么走的就是上面的  if (frame_queue_nb_remaining(&is->pictq) == 0)，此时跟 is->pictq.rindex_shown 一点关系没有，所以 is->pictq.rindex_shown 一直是 0。此时不会播放图片，符合预期。
         * 如果现在有图片了，那么走的就是 else 逻辑，else 里面一定会调用 frame_queue_next 方法，而这个方法会更新 is->pictq.rindex_shown 的值，此时is->pictq.rindex_shown 就会变成 1，就可以播放图片了，符合预期。
         * 如果之后一直没有图片了，那么又一直走的 if (frame_queue_nb_remaining(&is->pictq) == 0) 逻辑了，但是 is->pictq.rindex_shown 已经被更新为 1 了，而且frame队列里面有缓存过图片，此时可以一直播放最后一个画面，不会黑屏，符合预期。
         */
        if (is->force_refresh && is->pictq.rindex_shown)
            video_display(is);
    }
    /*
     * mark：如果显示过了，那么重新更新 force_refresh 的值，等到下一次显示的时候 force_refresh 再置为 1【即下一张图片播放的时候】。这是为了防止资源浪费，
     * 比如一个图片显示 5s，假设这个方法在5s内被调用40次，总不能每次都调用 video_display 重新画图吧，多浪费资源，这 5s 只要保证图片显示的是我们这一张图片就可以了。
     */
    is->force_refresh = 0;

    //    todo:步长的时候会导致 get_master_clock = NAN 会存在问题吧
    double clock = get_master_clock(is);
    if (!std::isnan(clock) && clock >= 0 && isCurrentSession(is)) {
        double media_time = clock;
        emit SigVideoPlaySeconds((int)media_time);
    }
}

// 视频同步流程，判断是否需要追赶音频，即需要本次追赶的话本张图片播放时间需要修改多少毫秒
double VideoCtrl::compute_target_delay(double delay, VideoState *is) {
    double sync_threshold, diff = 0;

    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        diff = get_clock(&is->vidclk) - get_master_clock(is);
        /*
         * mark：diff 是媒体时间单位（因为时钟存储的是原始 pts），需要转换为实时单位才能和 delay（实时单位）做运算。
         * 比如 2x 倍速下，diff=0.05s 媒体时间 = 0.025s 实时时间。
         * 不转换的话会过度补偿同步，导致画面卡顿。
         */
        double diff_real = diff / m_settings.rate();
        /*
         * mark：正常来说调整阈值不能超过 0.1s，不然肉眼能感受得出来了，我们这里的追赶是为了用户无法感知的情况下，实现音视频同步的调整。
         */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        /*
         * mark: fabs(diff) < is->max_frame_duration 代表的是我们认为两帧之间的时差允许的最大值，那么如果两帧之间的时差小于这个阈值，那么我们就认为这两帧之间的时差是有效的，那么我们就可以进行追赶。
         * 否则如果是 seek，从第一分钟跳转到第30分种，这很明显不需要我们追赶了，在外面会有额外的处理，比如直接丢帧得了。
         * todo：为什么音频没有这个判断？
         */
        if (!std::isnan(diff) && fabs(diff) < is->max_frame_duration) {
            /*
             * todo：这里追赶逻辑为什么是直接等很久，而不是每一帧调整一点，比如 delay = delay + diff 不是直接等同步完成了吗
             */
            if (diff_real <= -sync_threshold)
                delay = FFMAX(0, delay + diff_real);
            else if (diff_real >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff_real;
            else if (diff_real >= sync_threshold)
                delay = 2 * delay;
        }
    }
    av_log_trace("video: delay=%0.3f, A-V=%f\n", delay, -diff);
    // av_log_info("video_clk=%f, audio_clk=%f\n", get_clock(&is->vidclk), get_clock(&is->audclk));
    return delay;
}

double VideoCtrl::vp_duration(VideoState *is, Frame *vp, Frame *nextVp) {
    if (vp->serial == nextVp->serial) {
        double duration = nextVp->pts - vp->pts;
        // 如果差值无效，那么就只能使用帧本身的duration，可见 max_frame_duration 赋值地方的讲解
        if (std::isnan(duration) || duration <= 0 || duration > is->max_frame_duration) {
            /*
             * mark：视频的倍数功能就是这么简单，就是这一行代码就实现了，比如正常播放40ms，开启两倍速，那么就是外面 sleep 换成 20ms即可
             */
            return vp->duration / m_settings.rate();
        } else {
            return duration / m_settings.rate();
        }
    } else {
        return 0.0;
    }
}

void VideoCtrl::update_video_pts(VideoState *is, double pts, int64_t pos, int serial) {
    /*
     * mark：直接使用原始媒体时间 pts，不再除以倍率。
     * 倍速通过 Clock.speed 机制来处理，get_clock 会根据 speed 自动按倍率推进时间。
     * 之前除以倍率的方式会导致倍速切换时时钟跳变：
     *   比如倍率从1x变2x时，同一个 pts=2.0 的帧，时钟值从 2.0/1=2.0 突变到 2.0/2=1.0
     */
    set_clock(&is->vidclk, pts, serial);
}

void VideoCtrl::video_display(VideoState *is) {
    /*
    * mark：SDL相关知识可以查询目录下的SDL文档
    */
    if (!m_video_open)
        video_open();
    if (m_video_output_resources.renderer()) {
        if (g_show_rect_mutex.tryLock()) {
            m_video_output_resources.beginFrame();
            video_image_display(is);
            m_video_output_resources.endFrame();
            g_show_rect_mutex.unlock();
        }
    } else {
        qDebug() << "VideoCtrl::video_display - m_renderer is null!";
    }
}

void VideoCtrl::video_open() {
    int w,h;
    w = m_screen_width;
    h = m_screen_height;
    qDebug() << "VideoCtrl::video_open - m_play_wid:" << m_play_wid << "screen size:" << w << "x" << h;

    if (!m_video_output_resources.ensureWindow((void *)m_play_wid, &w, &h)) {
        av_log_error("SDL create window error");
        do_exit(m_cur_stream);
        return;
    }

    if (!m_video_output_resources.ensureRenderer()) {
        av_log_error("SDL create render error");
        do_exit(m_cur_stream);
        return;
    }

    m_cur_stream->width = w;
    m_cur_stream->height = h;
    m_video_open = true;//标记视频窗口已打开，即正在播放视频中
    qDebug() << "VideoCtrl::video_open - done, stream size:" << m_cur_stream->width << "x" << m_cur_stream->height;
}

void VideoCtrl::video_image_display(VideoState *is) {
    Frame *vp;
    SDL_Rect rect;
    vp = frame_queue_peek_last(&is->pictq);

    if (!vp) {
        qDebug() << "VideoCtrl::video_image_display - vp is NULL!";
        return;
    }

    // 设置显示区域【单纯是大小，不涉及图片内容】,同时窗口大小变动时，这个函数会重新计算显示区域，让图片同步窗口变大或变小
    calculate_display_rect(&rect, is->xleft, is->ytop, m_cur_stream->width, m_cur_stream->height, vp->width, vp->height, vp->sar);

    if (!vp->uploaded) {
        int sdlPixFmt = vp->frame->format == AV_PIX_FMT_YUV420P ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888;
        // 创建纹理（纹理由 VideoOutputResources 统一管理）
        if (!m_video_output_resources.ensureTexture(sdlPixFmt, vp->frame->width, vp->frame->height, SDL_BLENDMODE_NONE))
            return;
        // 更新纹理内容
        if (upload_texture(m_video_output_resources.texture(), vp->frame, is->owned_img_convert_ctx) < 0)
            return;
        vp->uploaded = 1;
        vp->flip_v = vp->frame->linesize[0] < 0;

        // 保存当前帧的宽高比和翻转信息，用于播放结束后窗口大小改变时重新渲染
        m_frame_sar = vp->sar;
        m_frame_flip_v = vp->flip_v;

        // 保存最后一帧的引用，用于播放结束后重建纹理（av_frame_ref 增加引用计数，保持数据有效）
        if (!m_last_frame)
            m_last_frame.reset(av_frame_alloc());
        av_frame_unref(m_last_frame.get());
        av_frame_ref(m_last_frame.get(), vp->frame);

        if (m_frame_width != vp->frame->width || m_frame_height != vp->frame->height) {
            m_frame_width = vp->frame->width;
            m_frame_height = vp->frame->height;
            qDebug() << "VideoCtrl::video_image_display - emitting SigFrameDimensionsChanged" << m_frame_width << m_frame_height;
            if (isCurrentSession(is)) {
                emit SigFrameDimensionsChanged(m_frame_width, m_frame_height);
            }
        }
    }
    /*mark：无论你图片有多大，我 rect 设置了多大，最后图片显示就是多大，即最后 SDL_RenderCopyEx 会帮我们做等比例缩放处理 */
    m_video_output_resources.presentTexture(rect, vp->flip_v);
}

void VideoCtrl::calculate_display_rect(SDL_Rect *rect, int src_x_left, int src_y_top, int src_width, int src_height,
                                      int pic_width, int pic_height, AVRational pic_sar) {
    float aspect_ratio;
    int width, height, x, y;
    if (pic_sar.num == 0)
        aspect_ratio = 0;
    else
        aspect_ratio = av_q2d(pic_sar);

    if (aspect_ratio <= 0.0)
        aspect_ratio = 1.0;
    // 比例以图片为准，实际宽高值以播放器界面为准
    aspect_ratio *= (float)pic_width / (float)pic_height;
    height = src_height;
    // & ~1 可以保证高度是偶数
    width = lrint(height * aspect_ratio) & ~1;
    if (width > src_width) {
        width = src_width;
        height = lrint(width / aspect_ratio) & ~1;
    }
    x = (src_width - width) / 2;
    y = (src_height - height) / 2;
    // 保证图片在界面中间
    rect->x = src_x_left + x;
    rect->y = src_y_top + y;
    rect->w = FFMAX(width, 1);
    rect->h = FFMAX(height, 1);
}

int VideoCtrl::upload_texture(SDL_Texture *tex, AVFrame *frame, SwsContextPtr &img_convert_ctx) {
    int ret = 0;
    switch (frame->format) {
    case AV_PIX_FMT_YUV420P:
        if (frame->linesize[0] < 0 || frame->linesize[1] < 0 || frame->linesize[2] < 0) {
            av_log_error("Negative linesize is not supported for YUV.\n");
            return -1;
        }
        ret = SDL_UpdateYUVTexture(tex, NULL, frame->data[0], frame->linesize[0],
                                   frame->data[1], frame->linesize[1],
                                   frame->data[2], frame->linesize[2]);
        break;
    case AV_PIX_FMT_BGRA:
        if (frame->linesize[0] < 0) {
            ret = SDL_UpdateTexture(tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
        }
        else {
            ret = SDL_UpdateTexture(tex, NULL, frame->data[0], frame->linesize[0]);
        }
        break;
    default:
        /* This should only happen if we are not using avfilter... */
        img_convert_ctx.reset(sws_getCachedContext(img_convert_ctx.release(),
                                                frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height,
                                                AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL));
        if (img_convert_ctx != nullptr) {
            uint8_t *pixels[4];
            int pitch[4];
            if (!SDL_LockTexture(tex, NULL, (void **)pixels, pitch)) {
                sws_scale(img_convert_ctx.get(), (const uint8_t * const *)frame->data, frame->linesize,
                          0, frame->height, pixels, pitch);
                SDL_UnlockTexture(tex);
            }
        }
        else {
            av_log_error("Cannot initialize the conversion context\n");
            ret = -1;
        }
        break;
    }
    return ret;
}

// 判断是否有足够的包，其实就是说还需不需要写入数据了，要不要等一等了
int VideoCtrl::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

bool VideoCtrl::get_playback_change() {
    return m_settings.rateChanged();
}

void VideoCtrl::set_playback_change(bool change) {
    m_settings.setRateChanged(change);
}

float VideoCtrl::get_playback_rate() {
    return m_settings.rate();
}

int64_t VideoCtrl::get_target_frequency() {
    if (m_cur_stream) {
        return m_cur_stream->audio_tgt.freq;
    }
    return NORMAL_SAMPLE_RATES;
}

int VideoCtrl::get_target_channels() {
    if (m_cur_stream) {
        return m_cur_stream->audio_tgt.channels;
    }
    return NORMAL_CHANNELS;
}

bool VideoCtrl::is_normal_playback_rate() {
    // 在 0.99 ~ 1.01 之间，那么就是正常播放速度，因为我们用的 float，它不一定计算出来刚好就是1，后面可能会有浮点数误差，所以这里加个范围判断
    if (m_settings.rate() > 0.99 && m_settings.rate() < 1.01) {
        return true;
    } else {
        return false;
    }
}

void VideoCtrl::OnSetSpeed(float speed)
{
    if (speed < PLAYBACK_RATE_MIN || speed > PLAYBACK_RATE_MAX) {
        return;
    }
    update_speed(speed);
}

void VideoCtrl::OnPause()
{
    if (m_cur_stream == nullptr) {
        // 如果现在在非播放状态，那么暂停/恢复播放按钮就变成了开启播放视频功能了【播放当前播放结束的视频】
        if (!m_current_file.isEmpty()) {
            start_play(m_current_file, m_play_wid);
        }
        return;
    }
    toggle_pause(m_cur_stream);
    emit SigPauseStat(m_cur_stream->paused);
}

void VideoCtrl::OnStop()
{
    qDebug() << "play stop";
    m_play_loop = false;
    m_stop_emitted = true; // 标记已发送停止信号

}

void VideoCtrl::OnUserStop()
{
    qDebug() << "user stop";
    m_user_stop = true; // 标记用户主动停止
    OnStop();
}

void VideoCtrl::OnPlayVolume(double percent)
{
    m_settings.setVolumeRatio(percent);
    if(m_cur_stream == nullptr) {
        return;
    }
    m_cur_stream->audio_volume = m_settings.volumeFor(SDL_MIX_MAXVOLUME);
}

void VideoCtrl::OnPlaySeek(double percent)
{
    if(m_cur_stream == nullptr) {
        return;
    }
    int64_t ts = percent * m_cur_stream->ic->duration;
    //    如果指定了从第几秒开始播放，那么seek的时候最小值需要加上这个时间
    if(m_cur_stream->ic->start_time != AV_NOPTS_VALUE) {
        ts += m_cur_stream->ic->start_time;
    }
    stream_seek(m_cur_stream, ts, 0);
}

void VideoCtrl::OnSeekForward()
{
    if (m_cur_stream == nullptr)
    {
        return;
    }

    double current_time = get_master_clock(m_cur_stream);
    int target_seconds = (int)(current_time + SEEK_INCR);
    stream_seek_forward();

    // 发出快进完成信号
    emit SigSeekForwardCompleted(target_seconds);
}

void VideoCtrl::OnSeekBack()
{
    if (m_cur_stream == nullptr)
    {
        return;
    }

    double current_time = get_master_clock(m_cur_stream);
    int target_seconds = (int)(current_time - SEEK_INCR);
    if (target_seconds < 0) target_seconds = 0;
    stream_seek_back();

    // 发出快退完成信号
    emit SigSeekBackCompleted(target_seconds);
}

void VideoCtrl::OnAddVolume()
{
    if (m_cur_stream == nullptr)
    {
        return;
    }
    add_volume();
}

void VideoCtrl::OnSubVolume()
{
    if (m_cur_stream == nullptr)
    {
        return;
    }
    sub_volume();
}

void VideoCtrl::OnStep()
{
    if (m_cur_stream == nullptr)
    {
        return;
    }
    step_to_next_frame(m_cur_stream);
}

void VideoCtrl::stream_toggle_pause(VideoState *is) {
    av_log_info("pause state changed, from:'%s' to:'%s'\n", is->paused ? "pause" : "not pause", !is->paused ? "pause" : "not pause");
    // 如果刚刚处于暂停状态，那么现在就需要恢复播放
    if (is->paused) {
        // 更新时间轴，现在恢复播放了，那么时间轴应该加上的暂停时间，在 video 显示的时候使用
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
        if (is->read_pause_return != AVERROR(ENOSYS))
            is->vidclk.paused = 0;
        // 更新视频时钟，其实pts没变，只更新了 time
        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
    }
    is->paused = is->audclk.paused = is->vidclk.paused = !is->paused;
    // 标记需要强制播放一帧音频
    m_settings.setForcePlay(false);
}

void VideoCtrl::toggle_pause(VideoState *is) {
    stream_toggle_pause(is);
    /*
     * mark：切换播放状态的时候，需要关闭逐帧，不考虑是变成暂停还是变成播放，一刀切即可，因为逐帧的时候会把 step 改为 1 的。
     * 比如播放变成暂停的时候，只是单纯暂停，跟逐帧没关系，所以这里把 step 改为 0，即关闭逐帧没什么影响
     * 当暂停变成播放的时候：
     * 1. 如果此时处于逐帧播放，那么恢复正常播放，需要关闭逐帧，即这里就应该将 step 改为 0
     * 2. 如果此时单纯暂停，恢复播放，跟逐帧也没关系，那么关闭逐帧也不会有问题
     */
    is->step = 0;
}

void VideoCtrl::step_to_next_frame(VideoState *is) {
    av_log_info("step start\n");
    /*
     * mark：如果当前是暂停状态，那么先恢复播放
     * 因为下面将 is->step 改为 1 了，就是开启了逐帧功能，在播放的时候如果检测到了开启了逐帧，那么播放一帧之后就会暂停
     */
    if (is->paused)
        stream_toggle_pause(is);
    is->step = 1;
}

/*
 * pos： 指定要跳转的时刻【注意，这个需要填的是文件中这个流的时间基转换的值，而不能是时间戳或者我们的时间基】
 * rel： 允许相对偏移量
 */
void VideoCtrl::stream_seek(VideoState *is, int64_t pos, int64_t rel) {
    double current_time = get_master_clock(is);
    int64_t target_seconds = pos / AV_TIME_BASE;
    av_log_info("seek start, from %02d:%02d:%02d to %02d:%02d:%02d\n",
                (int)current_time / 3600, ((int)current_time % 3600) / 60, (int)current_time % 60, (int)target_seconds / 3600, ((int)target_seconds % 3600) / 60, (int)target_seconds % 60);

    // 上一个 seek 请求处理完毕后才能再进行新的 seek 请求
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        // 指定不以字节的方式进行 seek【很明显那就是按照时间戳的方式来seek了】
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        // 不管现在因为什么阻塞住了，都需要结束阻塞，开始处理seek请求
        SDL_CondSignal(is->continue_read_thread);

        // // 联动 seek stem 源
        // if (m_audio_mode != AUDIO_ORIGINAL && m_stem.active) {
        //     seekStemSource(pos);
        // }
    }
}

void VideoCtrl::stream_seek_incr(int incr) {
    if (m_cur_stream == nullptr)
        return;

    double pos = get_master_clock(m_cur_stream);
    if (std::isnan(pos))
        // 转换为现实播放时长
        pos = (double)m_cur_stream->seek_pos / AV_TIME_BASE;
    // 最终要seek到的时间
    pos += incr;
    /*
     * mark：如果设置了开始时间，那么seek不可能到开始时间之前，故seek的最小值取开始时间
     */
    if (m_cur_stream->ic->start_time != AV_NOPTS_VALUE && pos < m_cur_stream->ic->start_time / (double)AV_TIME_BASE)
        pos = m_cur_stream->ic->start_time / (double)AV_TIME_BASE;
    // 需要将时间转换成流时间基对应值，所以需要 * AV_TIME_BASE
    stream_seek(m_cur_stream, (int64_t)(pos * AV_TIME_BASE), int64_t(incr * AV_TIME_BASE));
}

// 回退5s
void VideoCtrl::stream_seek_back() {
    double incr = -SEEK_INCR;
    stream_seek_incr(incr);
}

// 前进5s
void VideoCtrl::stream_seek_forward() {
    double incr = SEEK_INCR;
    stream_seek_incr(incr);
}

// 切换流通道
void VideoCtrl::stream_cycle_channel(int media_type) {
    AVFormatContext *ic = m_cur_stream->ic;
    int start_index, old_index, stream_index;
    AVStream *st;
    AVProgram *p = nullptr;
    int nb_streams = ic->nb_streams;

    if (media_type == AVMEDIA_TYPE_VIDEO) {
        old_index = start_index = m_cur_stream->last_video_stream;
    }
    else if (media_type == AVMEDIA_TYPE_AUDIO) {
        old_index = start_index = m_cur_stream->last_audio_stream;
    } else {
        // 暂无其他逻辑
    }
    stream_index = start_index;
    /*
     * mark：这是一个优化，忽略这个if也可以，假设：
     * 正常情况下一个电影，画面是一个画面，但是可能很多个音频版本，比如国语，粤语，英语，等等。那么这些音频应该是跟这一个视频画面绑定的，即它们应该属于一个 program
     * 那么如果在画面流不变的情况下，如果要切换音频，那么应该从当前画面的program中寻找，而不是从所有program中寻找，即不应该找到其他画面流的音频了。
     */
    if (media_type != AVMEDIA_TYPE_VIDEO && m_cur_stream->video_stream != -1) {
        p = av_find_program_from_stream(ic, nullptr, m_cur_stream->video_stream);
        if (p) {
            nb_streams = p->nb_stream_indexes;
            for (start_index = 0; start_index < nb_streams; start_index++) {
                /*
                 * mark：找到当前音频流在 program 中的索引，
                 * 如果我们取了 program，那么下面的 while 循环也是从 program 中找了，那么索引就不应该用 m_cur_stream->last_audio_stream ，因为这个是属于整个文件流的索引，而不是在 program 中的索引
                 */
                if (p->stream_index[start_index] == stream_index)
                    break;
            }
            // 如果找完了 program 都没找到这个音频流，就标记这个 program 中不存在音频流
            if (start_index == nb_streams) {
                start_index = -1;
            }
            // stream_index 是我们用来在下面 while 循环的一轮一轮找的索引，即先从 start_index 开始找，一点一点往后找
            stream_index = start_index;
        }
    }

    while (true) {
        // 因为刚进来的时候 stream_index 是当前流的索引，所以我们需要先++，拿到下一个索引
        stream_index++;
        if (stream_index >= nb_streams) {
            /*
             * mark：start_index == -1有两种情况
             * 1. 最开始就没打开音频流，那么就从0开始找，找了一圈都找不到可以使用的音频流，那么就退出【因为所有流都判断过了】
             * 2. 最开始有音频流，但是视频有 program，而 program 中没有音频流，那么就从0开始找，找了一圈都找不到可以使用的音频流，那么就退出【其实和上一点是一样的】
             */
            // 如果在找了一轮都找不到音频流的情况下，而且默认音频还不存在的情况下，那么直接退出【即保持先有状态，使用现在的音频流，或者现在没有音频流，那就继续没有】
            if (start_index == -1)
                return;
            // 这是一个圈，可能我们是从后面开始找的，前面几个元素还没找过，故跳转到队列的第一个元素
            stream_index = 0;
        }
        // 如果找到一圈都找不到下一个音频流，那么就退出
        if (stream_index == start_index)
            return;
        // 当有 program 时，program 的 stream_index[index] 中保存的是 index 在文件中的流的索引
        st = ic->streams[p ? p->stream_index[stream_index] : stream_index];
        // 如果找到了下一个流，那么就可以使用这个流了
        if (st->codecpar->codec_type == media_type) {
            switch (media_type) {
            case AVMEDIA_TYPE_AUDIO:
                // 如果是音频流的话，需要判断当前音频流是否能使用，如果采样率和通道数为0，那么很明显不能使用，那么继续找下一个流
                if (st->codecpar->sample_rate != 0 && st->codecpar->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
                goto the_end;
            default:
                break;
            }
        }
    }
the_end:
    // 如果当前索引是通过 program 找到的，那么需要将索引转换成文件流的真实索引
    if (p && stream_index != -1)
        stream_index = p->stream_index[stream_index];
    av_log_info("Switch %s stream from #%d to #%d\n", av_get_media_type_string((AVMediaType)media_type), old_index, stream_index);
    // 关闭老的流，打开新找到的流，就实现了流切换的功能了，而且新的流还是默认打开在当前播放位置，因为 read_thread 一直在读取包数据，此时 read_thread 读取到的数据就是此刻时间的数据
    stream_component_close(m_cur_stream, old_index);
    stream_component_open(m_cur_stream, stream_index);
}

void VideoCtrl::toggle_full_screen() {
    av_log_info("full screen state changed, from:'%s' to '%s'\n", m_is_full_screen ? "full" : "not full", !m_is_full_screen ? "full" : "not full");
    m_is_full_screen = !m_is_full_screen;
    m_video_output_resources.setFullScreen(m_is_full_screen);
    // 设置全屏后，需要刷新画面来同步大小改变事件
    m_cur_stream->force_refresh = 1;
}

/*
 * sing：
 *      1. 音量调大
 *      -1. 音量调小
 * step：音量调节的步长，单位是 db
 * 示例：
 * - 当前音量为 SDL_MIX_MAXVOLUME = 128（最大值），：按"音量+"键，增加 2dB：最终音量是 128（已达最大，故保持最大）
 * - 当前音量 64，按"音量-"键，减少 3dB： 最终音量从 64 降到 45
 */
void VideoCtrl::update_volume(int sign, double step) {
    /*
     * mark：这里 volume_level 和 new_volume 的计算是为了优化人耳听到音量变化的感觉，
     * 实际上可以直接写  m_cur_stream->audio_volume += 5 这样，每次固定增加或者减少 5 音量，但是固定的声音变化在不同声音大小情况下人耳感觉不一样。
     * 人耳对声音的感知是对数关系而非线性：
     *  线性音量从 100→90，感知变化很大
     *  线性音量从 10→0，感知变化很小
     *  使用 dB 刻度，每次增减相同的 dB 值，人耳感知的变化量基本一致
     *  这种设计让音量调节更符合人类听觉特性，用户体验更自然。
     * volume_level：将线性音量转换为分贝值
     * new_volume：分贝值增减后转换为线性音量，即最终给SDL播放的声音大小值
     * 最后的 av_clip: 防止在极小音量时因精度问题导致无法调节，如果计算前后值相同，则直接加减1。
     */
    if (m_cur_stream == nullptr)
    {
        return;
    }

    const int previous = m_cur_stream->audio_volume;
    m_cur_stream->audio_volume = m_settings.steppedVolume(previous, sign, step, SDL_MIX_MAXVOLUME);
    m_settings.setVolumeFrom(m_cur_stream->audio_volume, SDL_MIX_MAXVOLUME);
    av_log_info("volume changed, from:%d to %d\n", previous, m_cur_stream->audio_volume);

    emit SigVideoVolume(m_cur_stream->audio_volume * 1.0 / SDL_MIX_MAXVOLUME);
}

// 按照固定步长来增加音量
void VideoCtrl::add_volume() {
    update_volume(1, SDL_VOLUME_STEP);
}

// 按照固定步长来减少音量
void VideoCtrl::sub_volume() {
    update_volume(-1, SDL_VOLUME_STEP);
}

void VideoCtrl::update_speed(float speed) {
    av_log_info("playback rate changed, from:%f to:%f\n", m_settings.rate(), speed);
    // todo：其实可以做个优化，如果当前播放速度和目标播放速度相同，那么就不用切换播放速度了，这样会省点时间
    // 先更新倍率，再更新时钟速度。set_clock_speed 内部会先 get_clock 保存当前值，再修改 speed，避免时钟跳变
    m_settings.setRate(speed);
    if (m_cur_stream) {
        set_clock_speed(&m_cur_stream->vidclk, speed);
        set_clock_speed(&m_cur_stream->audclk, speed);
    }
    emit SigSpeed(m_settings.rate());
}

// ==================== 音频提取 ====================


bool VideoCtrl::OnExtractAudio(const QString &inputFile, const QString &outputFile)
{
    return m_audio_extraction_service.extract(inputFile, outputFile);
}

// ==================== 渲染目标切换（画中画） ====================

void VideoCtrl::OnSetRenderTarget(WId play_wid)
{
    if (play_wid == 0) {
        return;
    }
    if (!m_play_loop_thread.joinable()) {
        // 播放线程尚未启动：直接记录，下次开始播放时使用新的窗口
        m_play_wid = play_wid;
        return;
    }
    // 统一交由 SDL 线程处理，播放中、暂停和播放结束（空闲循环）三种情况都适用
    m_pending_wid.store(static_cast<quintptr>(play_wid));
    m_rebind_pending.store(true);
}

void VideoCtrl::applyPendingRenderTarget()
{
    if (!m_rebind_pending.exchange(false)) {
        return;
    }
    const WId wid = static_cast<WId>(m_pending_wid.exchange(0));
    if (wid == 0) {
        return;
    }

    if (wid != m_play_wid) {
        /*
         * 旧的 SDL 窗口包装的是已经被 Qt 销毁的原生窗口，继续在其上渲染会失败。
         * 因此整体释放窗口/渲染器/纹理，随后由 video_open()（有活动流）或
         * redrawLastFrame()（仅有最后一帧）在新的原生窗口上重建。
         */
        m_play_wid = wid;
        m_video_output_resources.reset();
        m_video_open = false;

        if (m_cur_stream) {
            // 纹理已释放，队列中标记为已上传的帧需要重新上传
            resetUploadedFlags(m_cur_stream);
            m_cur_stream->force_refresh = 1;
        }
        return;
    }

    // 句柄未变化：Qt 只是把同一个原生窗口迁移到了新的父窗口，无需重建渲染资源
    syncRenderTargetSize();
}

void VideoCtrl::syncRenderTargetSize()
{
    if (!m_video_output_resources.window()) {
        return;
    }

    int w = m_screen_width;
    int h = m_screen_height;
    m_video_output_resources.getWindowSize(&w, &h);
    if (w <= 0 || h <= 0) {
        return;
    }
    m_screen_width = w;
    m_screen_height = h;

    if (m_cur_stream) {
        m_cur_stream->width = w;
        m_cur_stream->height = h;
        m_cur_stream->force_refresh = 1;
        return;
    }

    // 播放已结束：直接按新的尺寸重绘最后一帧
    redrawLastFrame();
}

void VideoCtrl::resetUploadedFlags(VideoState *is)
{
    if (!is || !is->pictq.mutex) {
        return;
    }
    SDL_LockMutex(is->pictq.mutex);
    for (int i = 0; i < FRAME_QUEUE_SIZE; ++i) {
        is->pictq.queue[i].uploaded = 0;
    }
    SDL_UnlockMutex(is->pictq.mutex);
}

void VideoCtrl::redrawLastFrame()
{
    if (!m_last_frame || m_last_frame->width <= 0 || m_last_frame->height <= 0) {
        return;
    }

    int w = m_screen_width;
    int h = m_screen_height;
    if (!m_video_output_resources.ensureWindow((void *)m_play_wid, &w, &h)) {
        av_log_error("SDL create window error when rebinding render target\n");
        return;
    }
    m_screen_width = w;
    m_screen_height = h;

    if (!m_video_output_resources.ensureRenderer()) {
        av_log_error("SDL create render error when rebinding render target\n");
        return;
    }

    const int sdlPixFmt = m_last_frame->format == AV_PIX_FMT_YUV420P ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888;
    if (!m_video_output_resources.ensureTexture(sdlPixFmt, m_last_frame->width, m_last_frame->height, SDL_BLENDMODE_NONE)) {
        return;
    }

    SwsContextPtr sws_ctx;
    if (upload_texture(m_video_output_resources.texture(), m_last_frame.get(), sws_ctx) < 0) {
        return;
    }

    m_frame_width = m_last_frame->width;
    m_frame_height = m_last_frame->height;
    m_frame_sar = m_last_frame->sample_aspect_ratio;

    if (g_show_rect_mutex.tryLock()) {
        m_video_output_resources.beginFrame();
        SDL_Rect rect;
        calculate_display_rect(&rect, 0, 0, m_screen_width, m_screen_height,
                               m_frame_width, m_frame_height, m_frame_sar);
        m_video_output_resources.presentTexture(rect, m_frame_flip_v);
        m_video_output_resources.endFrame();
        g_show_rect_mutex.unlock();
    }
}