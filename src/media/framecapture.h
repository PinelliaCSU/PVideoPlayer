#ifndef FRAMECAPTURE_H
#define FRAMECAPTURE_H

#include <QString>

struct AVFrame;

/*
 * 视频帧截图：把解码后的一帧写成图片文件。
 *
 * 帧到图片的转换与编码都收敛在这里，界面层只拿到结果路径，
 * 不需要接触 FFmpeg 的帧结构（见 update.md 的截图功能说明）。
 */
namespace FrameCapture
{
struct Result
{
    bool success = false;
    QString path;  // 实际写出的文件路径（平台缺少所需格式插件时可能回退为 PNG）
    QString error; // 失败原因
};

// 依 filePath 的扩展名选择格式（png / jpg / jpeg / bmp），其余按 PNG 处理
Result saveFrame(const AVFrame *frame, const QString &filePath);
} // namespace FrameCapture

#endif // FRAMECAPTURE_H
