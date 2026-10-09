#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QWidget>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>

class PlaylistModel;

namespace Ui {
class Playlist;
}

/*
 * 播放列表视图：只负责列表控件的展示与用户操作转发。
 * 列表数据由注入的 PlaylistModel 提供；增删、校验、播放模式和持久化
 * 由 AppController / PlaylistCoordinator 处理，本类不持有播放业务状态。
 */
class Playlist : public QWidget
{
    Q_OBJECT

public:
    explicit Playlist(QWidget *parent = nullptr);
    ~Playlist();

    bool Init();
    // 绑定列表数据模型（模型生命周期由应用控制器管理）
    void SetPlaylistModel(PlaylistModel *model);
    // 同步当前选中项（由控制器在播放索引变化时调用）
    void SetCurrentIndex(int row);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

signals:
    // 用户要求把某个地址加入播放列表（未校验的原始输入）
    void SigAddRequested(const QString &locator);
    // 用户要求播放某一行
    void SigPlayIndexRequested(int row);
    void SigRemoveRequested(int row);
    void SigClearRequested();

private:
    bool initUi();
    void connectSignalSlots();

    Ui::Playlist *ui;
    PlaylistModel *_model = nullptr;
};

#endif // PLAYLIST_H
