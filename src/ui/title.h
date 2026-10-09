#ifndef TITLE_H
#define TITLE_H

#include <QWidget>

namespace Ui {
class Title;
}

class Title : public QWidget
{
    Q_OBJECT

public:
    explicit Title(QWidget *parent = nullptr);
    ~Title();
    bool Init();
    void SlotOnPlay(QString filePath);
    void SlotOnStop(); // 用户主动停止时清空文件名
private:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    bool initUi();
    void connectSignalSlots();
signals:
    void SigMinBtnClicked();
    void SigMaxBtnClicked();
    void SigFullScreenBtnClicked();
    void SigCloseBtnClicked();
    void SigMenuBtnClicked();
private:
    Ui::Title *ui;
};

#endif // TITLE_H
