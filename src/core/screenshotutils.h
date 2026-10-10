#ifndef SCREENSHOTUTILS_H
#define SCREENSHOTUTILS_H

#include <QDateTime>
#include <QString>

/*
 * 截图路径工具：文件名规则与目录解析。
 * 只做纯字符串/路径处理，不涉及视频帧，便于单元测试与界面复用。
 */
namespace ScreenshotUtils
{
// 默认截图目录：系统图片目录下的 PVideoPlayer 子目录
QString defaultDirectory();

// 媒体显示名：取文件名并去掉目录、扩展名，替换文件名非法字符
QString mediaDisplayName(const QString &mediaLocator);

// 截图文件名：视频名 + 时间戳，例如 sample_20261010_080130.png
QString buildFileName(const QString &mediaLocator, const QDateTime &timestamp,
                      const QString &extension);

// 生成不冲突的完整路径：同名文件已存在时追加 -1、-2 序号
QString uniquePath(const QString &directory, const QString &fileName);
} // namespace ScreenshotUtils

#endif // SCREENSHOTUTILS_H
