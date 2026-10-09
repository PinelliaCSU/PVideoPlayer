#include "playbacksettings.h"

#include <algorithm>
#include <cmath>

float PlaybackSettings::rate() const
{
    return m_rate.load();
}

bool PlaybackSettings::isNormalRate() const
{
    // 浮点误差导致倍率不会刚好等于 1，因此用一个小区间判定正常倍速
    const float rate = m_rate.load();
    return rate > 0.99f && rate < 1.01f;
}

bool PlaybackSettings::isRateSupported(float rate) const
{
    return !(rate < PLAYBACK_RATE_MIN || rate > PLAYBACK_RATE_MAX);
}

void PlaybackSettings::setRate(float rate)
{
    m_rate.store(rate);
    m_rateChanged.store(true);
}

bool PlaybackSettings::rateChanged() const
{
    return m_rateChanged.load();
}

void PlaybackSettings::setRateChanged(bool changed)
{
    m_rateChanged.store(changed);
}

double PlaybackSettings::volumeRatio() const
{
    return m_volumeRatio.load();
}

void PlaybackSettings::setVolumeRatio(double ratio)
{
    m_volumeRatio.store(std::clamp(ratio, 0.0, 1.0));
}

int PlaybackSettings::volumeFor(int maxVolume) const
{
    if (maxVolume <= 0) {
        return 0;
    }
    const double scaled = m_volumeRatio.load() * maxVolume;
    return std::clamp(static_cast<int>(std::lround(scaled)), 0, maxVolume);
}

void PlaybackSettings::setVolumeFrom(int value, int maxVolume)
{
    if (maxVolume <= 0) {
        m_volumeRatio.store(0.0);
        return;
    }
    m_volumeRatio.store(std::clamp(static_cast<double>(value) / maxVolume, 0.0, 1.0));
}

int PlaybackSettings::steppedVolume(int currentValue, int sign, double step, int maxVolume) const
{
    if (maxVolume <= 0) {
        return 0;
    }

    /*
     * 人耳对音量的感知接近对数关系，因此按 dB 步长调节而不是线性加减，
     * 这样在不同音量下听到的变化幅度基本一致。
     */
    const double current = std::clamp(currentValue, 0, maxVolume);
    const double level = current > 0.0 ? (20.0 * std::log(current / maxVolume) / std::log(10.0)) : -1000.0;
    const int target = static_cast<int>(std::lround(maxVolume * std::pow(10.0, (level + sign * step) / 20.0)));
    // 极小音量时 dB 换算可能取整为同一个值，此时按符号步进一格保证音量可调
    const int stepped = (static_cast<int>(current) == target) ? (static_cast<int>(current) + sign)
                                                              : target;
    return std::clamp(stepped, 0, maxVolume);
}

bool PlaybackSettings::forcePlay() const
{
    return m_forcePlay.load();
}

void PlaybackSettings::setForcePlay(bool force)
{
    m_forcePlay.store(force);
}
