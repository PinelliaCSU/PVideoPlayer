#ifndef MEDIAINFODIALOG_H
#define MEDIAINFODIALOG_H

#include <QDialog>
#include <QList>
#include <QPoint>
#include <QTimer>

#include <functional>

#include "media_types.h"

class QGridLayout;
class QLabel;
class QWidget;

struct MediaInfoField;

/*
 * 播放信息面板：以深色无边框弹窗展示当前媒体的详细信息，
 * 样式与播放器整体风格保持一致（见 res/qss/mediainfo.css）。
 *
 * 面板打开期间通过 setInfoProvider() 周期性重新读取媒体信息，
 * 缓冲、丢帧等统计随播放实时变化，而不是停留在打开瞬间的快照。
 */
class MediaInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MediaInfoDialog(QWidget *parent = nullptr);

    // 填充媒体信息；info.valid 为 false 时提示当前没有正在播放的媒体
    void setMediaInfo(const MediaInfo &info);

    /*
     * 设置实时信息提供者：设置后立即刷新一次，并周期性重新取值；
     * 传入空函数对象可停止刷新。
     */
    void setInfoProvider(std::function<MediaInfo()> provider);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // 更新展示内容：字段结构变化时重建行，否则只刷新取值
    void applyFields(const QList<MediaInfoField> &fields);
    void clearRows();
    void buildRows(const QList<MediaInfoField> &fields);
    void refreshValues(const QList<MediaInfoField> &fields);

    QWidget *_titleBar = nullptr;
    QGridLayout *_fieldLayout = nullptr;
    QList<QLabel *> _valueLabels; // 与字段行一一对应的取值控件
    QTimer _refreshTimer;
    std::function<MediaInfo()> _infoProvider;
    bool _showingPlaceholder = false;
    bool _dragging = false;
    bool _centered = false;
    QPoint _dragOffset;
};

#endif // MEDIAINFODIALOG_H
