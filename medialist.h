#ifndef MEDIALIST_H
#define MEDIALIST_H

#include <QListWidget>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>

class MediaList : public QListWidget
{
    Q_OBJECT
public:
    explicit MediaList(QWidget *parent = nullptr);
    ~MediaList();
    bool Init();
protected:
    //右键触发显示菜单
    void contextMenuEvent(QContextMenuEvent *);
signals:
    void SigAddFile(QString filePath);
    void SigRemoveFile(int row);
    void SigClearList();
private:
    bool initUi();
    void addFile();
    void removeFile();
private:
    QMenu _menu;
    QAction _act_add_file;
    QAction _act_remove_file;
    QAction _act_clear_list;
};

#endif // MEDIALIST_H
