#ifndef CUSTOMSLIDER_H
#define CUSTOMSLIDER_H


#include<QSlider>
#include<QMouseEvent>

#include <QSlider>

class CustomSlider: public QSlider
{
    Q_OBJECT
public:
    CustomSlider(QWidget *parent=nullptr);
    ~CustomSlider();
protected:
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
signals:
    void SigSliderValueChanged();   //处理鼠标点击事件
private:
    bool _pressed; //是否按住按钮中
};

#endif // CUSTOMSLIDER_H
