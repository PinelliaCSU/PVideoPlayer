#ifndef TST_MEDIAINFODIALOG_H
#define TST_MEDIAINFODIALOG_H

#include <QObject>

// 播放信息面板的回归测试：
// 1) 面板在“占位提示”和“媒体字段”之间切换时，旧内容必须被彻底移除；
// 2) 设置了信息提供者后，取值随周期刷新实时变化，且不重建控件。
class MediaInfoDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void placeholderIsRemovedWhenFieldsAreShown();
    void fieldsAreRemovedWhenPlaceholderIsShown();
    void infoProviderKeepsValuesLive();
};

#endif // TST_MEDIAINFODIALOG_H
