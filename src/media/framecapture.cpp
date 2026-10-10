#include "framecapture.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#include "media_raii.h"

namespace {

const char kPngFormat[] = "PNG";

// 统一转换到 RGB24：Qt 的图片编码器可以直接使用
constexpr AVPixelFormat kTargetFormat = AV_PIX_FMT_RGB24;

QString imageFormatForSuffix(const QString &suffix)
{
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        return QStringLiteral("JPEG");
    }
    if (suffix == QLatin1String("bmp")) {
        return QStringLiteral("BMP");
    }
    return QString::fromLatin1(kPngFormat);
}

} // namespace

FrameCapture::Result FrameCapture::saveFrame(const AVFrame *frame, const QString &filePath)
{
    Result result;
    result.path = filePath;

    if (frame == nullptr || frame->width <= 0 || frame->height <= 0 || frame->data[0] == nullptr) {
        result.error = QCoreApplication::translate("FrameCapture", "没有可用的视频帧");
        return result;
    }

    const QFileInfo target(filePath);
    const QDir directory = target.absoluteDir();
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath())) {
        result.error = QCoreApplication::translate("FrameCapture", "无法创建截图目录");
        return result;
    }

    // 帧的像素格式由解码器决定，截图统一转成 RGB24 后再交给 Qt 编码
    SwsContextPtr scaler(sws_getContext(frame->width, frame->height,
                                        static_cast<AVPixelFormat>(frame->format),
                                        frame->width, frame->height, kTargetFormat,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!scaler) {
        result.error = QCoreApplication::translate("FrameCapture", "无法创建像素格式转换上下文");
        return result;
    }

    QImage image(frame->width, frame->height, QImage::Format_RGB888);
    if (image.isNull()) {
        result.error = QCoreApplication::translate("FrameCapture", "无法分配截图缓冲区");
        return result;
    }

    uint8_t *destination[4] = { image.bits(), nullptr, nullptr, nullptr };
    int destinationLinesize[4] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };
    sws_scale(scaler.get(), frame->data, frame->linesize, 0, frame->height,
              destination, destinationLinesize);

    const QString format = imageFormatForSuffix(target.suffix().toLower());
    if (image.save(result.path, format.toLatin1().constData())) {
        result.success = true;
        return result;
    }

    /*
     * 平台缺少该格式的图片插件（部署环境很常见）时回退为 PNG，
     * 宁可换格式保存，也不要让用户的截图直接失败。
     */
    if (format != QLatin1String(kPngFormat)) {
        result.path = directory.filePath(target.completeBaseName() + QLatin1String(".png"));
        if (image.save(result.path, kPngFormat)) {
            result.success = true;
            return result;
        }
    }

    result.error = QCoreApplication::translate("FrameCapture", "图片写入失败，请检查保存目录是否可写");
    return result;
}
