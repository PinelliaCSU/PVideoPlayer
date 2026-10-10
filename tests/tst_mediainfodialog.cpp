#include "tst_mediainfodialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QtTest>

#include "../src/ui/mediainfodialog.h"

namespace {

MediaInfo playingInfo()
{
    MediaInfo info;
    info.valid = true;
    info.fileName = QStringLiteral("sample.mkv");
    info.width = 1920;
    info.height = 1080;
    return info;
}

// 按字段名取出面板上的取值：字段名和取值始终在同一行的两列里
QString valueOf(const MediaInfoDialog &dialog, const QString &label)
{
    const QGridLayout *grid = dialog.findChild<QGridLayout *>();
    if (grid == nullptr) {
        return QString();
    }
    for (int row = 0; row < grid->rowCount(); ++row) {
        const QLayoutItem *labelItem = grid->itemAtPosition(row, 0);
        const QLayoutItem *valueItem = grid->itemAtPosition(row, 1);
        if (labelItem == nullptr || valueItem == nullptr) {
            continue;
        }
        const auto *labelWidget = qobject_cast<QLabel *>(labelItem->widget());
        const auto *valueWidget = qobject_cast<QLabel *>(valueItem->widget());
        if (labelWidget != nullptr && valueWidget != nullptr && labelWidget->text() == label) {
            return valueWidget->text();
        }
    }
    return QString();
}

} // namespace

void MediaInfoDialogTest::placeholderIsRemovedWhenFieldsAreShown()
{
    MediaInfoDialog dialog;
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("MediaInfoHint")) != nullptr);

    dialog.setMediaInfo(playingInfo());

    // 占位提示如果只是延后销毁（deleteLater），会和字段行同时显示
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("MediaInfoHint")) == nullptr);
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("MediaInfoFieldLabel")) != nullptr);
}

void MediaInfoDialogTest::fieldsAreRemovedWhenPlaceholderIsShown()
{
    MediaInfoDialog dialog;
    dialog.setMediaInfo(playingInfo());

    dialog.setMediaInfo(MediaInfo{});

    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("MediaInfoFieldLabel")) == nullptr);
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("MediaInfoFieldValue")) == nullptr);
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("MediaInfoHint")) != nullptr);
}

void MediaInfoDialogTest::infoProviderKeepsValuesLive()
{
    MediaInfo info = playingInfo();
    info.videoBufferFrames = 1;
    info.droppedFrames = 0;

    MediaInfoDialog dialog;
    dialog.setInfoProvider([&info]() { return info; });

    // 设置提供者后立即取一次值，不必等到第一次超时
    QVERIFY(valueOf(dialog, QStringLiteral("当前播放缓冲状态")).contains(QStringLiteral("视频 1 帧")));

    // 播放推进后，周期刷新应把新的缓冲统计反映到面板上
    info.videoBufferFrames = 7;
    info.droppedFrames = 3;
    QTest::qWait(800);

    const QString status = valueOf(dialog, QStringLiteral("当前播放缓冲状态"));
    QVERIFY(status.contains(QStringLiteral("视频 7 帧")));
    QVERIFY(status.contains(QStringLiteral("累计丢帧 3")));

    // 刷新只更新取值文本，不重建行，避免面板抖动
    QCOMPARE(dialog.findChildren<QLabel *>(QStringLiteral("MediaInfoFieldValue")).size(), 10);
}

