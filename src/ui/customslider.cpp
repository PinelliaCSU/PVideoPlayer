#include "customslider.h"
#include "configutils.h"

CustomSlider::CustomSlider(QWidget *parent):
    QSlider(parent),
    _pressed(false)
{
    this->setMaximum(ConfigUtils::MAX_SLIDER_VALUE);
}


CustomSlider::~CustomSlider()
{

}


void CustomSlider::mousePressEvent(QMouseEvent *event){
    QSlider::mousePressEvent(event);

    _pressed = true;

    double pos = event->pos().x() / (double)width();
    setValue(pos * (maximum() - minimum()) + minimum());

    emit SigSliderValueChanged();
}

void CustomSlider::mouseReleaseEvent(QMouseEvent *event)
{

    QSlider::mouseReleaseEvent(event);
    _pressed = false;
    //    todo：如果是视频播放的话，那么按道理应该是在松开的时候进行视频的seek，故视频的seek信号应该在这里触发，拖动和按下的时候不需要发送信号才对
    //    emit SigSliderValueChanged();
}


void CustomSlider::mouseMoveEvent(QMouseEvent *event)
{

    QSlider::mouseMoveEvent(event);

    if(_pressed) {
        double pos = event->pos().x() / (double)width();
        setValue(pos * (maximum() - minimum()) + minimum());
        emit SigSliderValueChanged();
    }
}
