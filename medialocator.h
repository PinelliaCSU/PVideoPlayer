#ifndef MEDIALOCATOR_H
#define MEDIALOCATOR_H

#include <QString>

// 媒体地址（本地文件路径或网络流 URL）的统一解析与校验入口，
// 避免播放列表与拖放路径各自维护重复的校验逻辑。
namespace MediaLocator
{
// 是否是播放器支持的网络流地址
bool isNetworkStream(const QString &locator);

// 本地文件扩展名是否属于支持的媒体格式
bool isSupportedMediaFile(const QString &locator);

enum class LocatorStatus
{
    Valid,             // 可以作为播放地址使用
    Empty,             // 地址为空
    UnsupportedFormat, // 本地扩展名不受支持（仅在 requireSupportedFormat 为真时返回）
    FileNotExists      // 本地文件不存在
};

// 归一化并校验媒体地址：
// - 网络流：原样返回，isNetworkStream 置为 true；
// - 本地文件：按需校验扩展名与存在性，成功时返回绝对路径。
LocatorStatus normalize(const QString &input,
                        bool requireSupportedFormat,
                        QString *locator,
                        bool *isNetworkStream);
} // namespace MediaLocator

#endif // MEDIALOCATOR_H
