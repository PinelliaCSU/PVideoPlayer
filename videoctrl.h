#ifndef VIDEOCTRL_H
#define VIDEOCTRL_H

#include <QObject>
#include <QWidget>
#include<QThread>
#include<QString>

#include <atomic>




#include "datactrl.h"
#include "sonic.h"
#include "playbackbackend.h"
#include "clockcontroller.h"
#include "media_raii.h"
#include "audioextractionservice.h"
#include "mediacomponents.h"


#define PLAYBACK_RATE_MIN           (0.25)     // 最慢
#define PLAYBACK_RATE_MAX           (3.0)     // 最快
#define PLAYBACK_RATE_RESET         (1.0)     //默认播放速度
#define PLAYBACK_RATE_SCALE         (0.25)    // 变速刻度
#define NORMAL_SAMPLE_RATES         (44100)  // 默认采样率
#define NORMAL_CHANNELS             (2)     // 默认声道数

class VideoCtrl : public PlaybackEventSource, public IPlaybackBackend
{
    Q_OBJECT
public:

    void start_play(QString filename, WId play_wid) override;
    ~VideoCtrl();
    static VideoCtrl *GetInstance();

    int audio_decode_frame(VideoState *is);
    void set_clock_at(Clock *c, double pts, int serial, double time);
    double get_clock(Clock *c);
    bool get_playback_change();
    void set_playback_change(bool change);
    float get_playback_rate();
    int64_t get_target_frequency();
    int get_target_channels();
    bool is_normal_playback_rate();
    void OnSetSpeed(float speed) override; // 设置指定倍速
    void OnPause() override;
    void OnStop() override;
    void OnUserStop() override; // 用户主动点击停止按钮
    void OnPlayVolume(double percent) override;
    void OnPlaySeek(double percent) override;
    void OnSeekForward() override;
    void OnSeekBack() override;
    void OnAddVolume() override;
    void OnSubVolume() override;
    void OnStep() override; // 逐帧播放

    //获取音频
    bool OnExtractAudio(const QString &inputFile, const QString &outputFile) override;

    // 切换渲染目标原生窗口（画中画）：保持当前播放会话，在新的窗口上重建渲染资源
    void OnSetRenderTarget(WId play_wid) override;
signals:
    void SigStop();
private:
    explicit VideoCtrl(QObject *parent = nullptr);

    bool init();
    bool ConnectionSignalSlots();
    VideoState* stream_open(const char* filename);
    void init_clock(Clock *c, int *queueSerial);
    void set_clock(Clock *c, double pts, int serial);
    void set_clock_speed(Clock *c, double speed);
    double get_master_clock(VideoState *is);
    void read_thread(VideoState *is);
    int stream_component_open(VideoState *is, int stream_index);
    int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
    int synchronize_audio(VideoState *is, int nb_samples);
    int get_master_sync_type(VideoState *is);
    int audio_thread(void *arg);
    int video_thread(void *arg);
    int get_video_frame(VideoState *is,AVFrame *frame);
    int queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
    void do_exit(VideoState *is);
    void stream_close(VideoState *is);
    void stream_component_close(VideoState *is, int stream_index);
    void loop_thread(VideoState *curStream);
    void refresh_loop_wait_event(VideoState *is, SDL_Event *event);
    void video_refresh(void *arg, double *remainingTime);
    double compute_target_delay(double delay, VideoState *is);
    double vp_duration(VideoState *is, Frame *vp, Frame *nextVp);
    void update_video_pts(VideoState *is, double pts, int64_t pos, int serial);
    void video_display(VideoState *is);
    void video_open();
    void video_image_display(VideoState *is);
    void calculate_display_rect(SDL_Rect *rect, int src_x_left, int src_y_top, int src_width, int src_height, int pic_width, int pic_height, AVRational pic_sar);
    int upload_texture(SDL_Texture *tex, AVFrame *frame, SwsContextPtr &img_convert_ctx);
    int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);

    void stream_toggle_pause(VideoState *is);
    void toggle_pause(VideoState *is);
    void step_to_next_frame(VideoState *is);
    void stream_seek(VideoState *is, int64_t pos, int64_t rel);
    void stream_seek_incr(int incr);
    void stream_seek_back();
    void stream_seek_forward();
    void stream_cycle_channel(int media_type);

    // 渲染目标切换：applyPendingRenderTarget 必须运行在 SDL 线程（loop_thread）
    void applyPendingRenderTarget();
    // 原生窗口句柄未变（Qt 迁移了同一个窗口）时，按新窗口尺寸同步渲染区域
    void syncRenderTargetSize();
    // 播放已结束（无活动流）时，在新的渲染目标上重绘最后一帧
    void redrawLastFrame();
    static void resetUploadedFlags(VideoState *is);

    void toggle_full_screen();
    void update_volume(int sign, double step);
    void add_volume();
    void sub_volume();
    void update_speed(float speed);
private:
    static VideoCtrl* m_instance;
    bool m_init;
    bool m_play_loop;
    std::thread m_play_loop_thread;
    VideoState* m_cur_stream;

    int m_screen_width;
    int m_screen_height;

    int m_frame_width;
    int m_frame_height;

    int m_startup_volume;
    bool m_is_full_screen;

    float m_playback_rate; //播放速度
    bool m_playback_changed; //播放速度是否改变
    WId m_play_wid;//播放窗口
    bool m_stop_emitted; //标记是否已经发送过停止信号
    QString m_current_file; // 当前播放文件路径
    bool m_video_open; // 视频窗口打开状态
    AVRational m_frame_sar;            // 当前帧的宽高比
    bool m_frame_flip_v;               // 当前帧是否垂直翻转
    AvFramePtr m_last_frame;             // 最后一帧的引用由 RAII 管理
    bool m_idle_loop;                   // 空闲事件循环运行标志（true=运行中，类似 m_play_loop）
    bool m_user_stop;                   // 用户主动停止标志（区分自然播放结束和用户点击停止）
    ClockController m_clock_controller;
    AudioExtractionService m_audio_extraction_service;
    AudioOutputDevice m_audio_output;
    VideoOutputResources m_video_output_resources;  // 窗口、渲染器、纹理与渲染原语的所有者

    // 渲染目标切换请求（Qt 主线程写入，SDL 线程消费）
    std::atomic<bool> m_rebind_pending{false};
    std::atomic<quintptr> m_pending_wid{0};

    bool m_audio_force_play = true;//音频强制播放一帧，配合step使用
public:
    sonicStreamStruct* m_audio_speed_convert;
};

#endif // VIDEOCTRL_H
