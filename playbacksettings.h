#ifndef PLAYBACKSETTINGS_H
#define PLAYBACKSETTINGS_H

#include <atomic>

#define PLAYBACK_RATE_MIN           (0.25)     // 最慢
#define PLAYBACK_RATE_MAX           (3.0)     // 最快
#define PLAYBACK_RATE_RESET         (1.0)     //默认播放速度
#define PLAYBACK_RATE_SCALE         (0.25)    // 变速刻度

/*
 * 播放参数（倍速、音量、逐帧强制播放标记）的唯一持有者。
 *
 * 与 SDL/FFmpeg 解耦：音量以 0.0~1.0 的比值表示，由调用方换算到具体的音量刻度；
 * 倍速变化标记用于通知音频回调重建 sonic 变速流。这些字段会被命令线程、
 * 音频回调和渲染线程同时访问，因此统一使用原子成员。
 */
class PlaybackSettings final
{
public:
    float rate() const;
    bool isNormalRate() const;
    bool isRateSupported(float rate) const;
    // 记录新的倍率，并置位“倍率已变化”标记
    void setRate(float rate);

    bool rateChanged() const;
    void setRateChanged(bool changed);

    double volumeRatio() const;
    void setVolumeRatio(double ratio);
    int volumeFor(int maxVolume) const;
    void setVolumeFrom(int value, int maxVolume);
    // 按 dB 步长增减音量，返回新的音量刻度值（0~maxVolume）
    int steppedVolume(int currentValue, int sign, double step, int maxVolume) const;

    bool forcePlay() const;
    void setForcePlay(bool force);

private:
    std::atomic<float> m_rate{PLAYBACK_RATE_RESET};
    std::atomic<bool> m_rateChanged{false};
    std::atomic<double> m_volumeRatio{0.5};
    std::atomic<bool> m_forcePlay{true};
};

#endif // PLAYBACKSETTINGS_H
