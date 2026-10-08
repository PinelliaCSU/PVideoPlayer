#include "guiUtils.h"
#include <QFile>
#include <QFont>
#include <QDebug>

namespace GuiUtils
{
QString LoadQss(const QString& qssPath)
{
    QFile qssFile(qssPath);
    if (!qssFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to read stylesheet:" << qssPath;
        return QString();
    }
    return QLatin1String(qssFile.readAll());
}

void SetIcon(QPushButton *btn, int iconSize, QChar icon)
{
    QFont font("FontAwesome"); // 确保这个字体已经加载
    font.setPointSize(iconSize);
    btn->setFont(font);
    btn->setText(icon);
}

QString GetKeyName(int key)
{
    QString keyName;
    switch (key) {
    case Qt::Key_Space: keyName = "Space"; break;
    case Qt::Key_Escape: keyName = "ESC"; break;
    case Qt::Key_F1: keyName = "F1"; break;
    case Qt::Key_F2: keyName = "F2"; break;
    case Qt::Key_F3: keyName = "F3"; break;
    case Qt::Key_F4: keyName = "F4"; break;
    case Qt::Key_F5: keyName = "F5"; break;
    case Qt::Key_F6: keyName = "F6"; break;
    case Qt::Key_F7: keyName = "F7"; break;
    case Qt::Key_F8: keyName = "F8"; break;
    case Qt::Key_F9: keyName = "F9"; break;
    case Qt::Key_F10: keyName = "F10"; break;
    case Qt::Key_F11: keyName = "F11"; break;
    case Qt::Key_F12: keyName = "F12"; break;
    case Qt::Key_Left: keyName = "Left"; break;
    case Qt::Key_Right: keyName = "Right"; break;
    case Qt::Key_Up: keyName = "Up"; break;
    case Qt::Key_Down: keyName = "Down"; break;
    case Qt::Key_Enter: keyName = "Enter"; break;
    case Qt::Key_Return: keyName = "Return"; break;
    case Qt::Key_Backspace: keyName = "Backspace"; break;
    case Qt::Key_Tab: keyName = "Tab"; break;
    case Qt::Key_Delete: keyName = "Delete"; break;
    case Qt::Key_Insert: keyName = "Insert"; break;
    case Qt::Key_Home: keyName = "Home"; break;
    case Qt::Key_End: keyName = "End"; break;
    case Qt::Key_PageUp: keyName = "PageUp"; break;
    case Qt::Key_PageDown: keyName = "PageDown"; break;
    case Qt::Key_CapsLock: keyName = "CapsLock"; break;
    case Qt::Key_NumLock: keyName = "NumLock"; break;
    case Qt::Key_ScrollLock: keyName = "ScrollLock"; break;
    case Qt::Key_Print: keyName = "Print"; break;
    case Qt::Key_Pause: keyName = "Pause"; break;
    case Qt::Key_SysReq: keyName = "SysReq"; break;
    case Qt::Key_Clear: keyName = "Clear"; break;
    default:
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            keyName = QChar('A' + key - Qt::Key_A);
        } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            keyName = QChar('0' + key - Qt::Key_0);
        } else if (key >= Qt::Key_Asterisk && key <= Qt::Key_division) {
            // 小键盘数字
            keyName = QString("Num%1").arg(key - Qt::Key_0);
        } else {
            keyName = QString::number(key);
        }
        break;
    }
    return keyName;
}

bool CheckNetworkStream(const QString &fileName)
{
        bool isNetworkStream = fileName.startsWith("http://", Qt::CaseInsensitive) ||
                               fileName.startsWith("https://", Qt::CaseInsensitive) ||
                               fileName.startsWith("rtmp://", Qt::CaseInsensitive) ||
                               fileName.startsWith("rtsp://", Qt::CaseInsensitive) ||
                               fileName.startsWith("mms://", Qt::CaseInsensitive) ||
                               fileName.startsWith("mmsh://", Qt::CaseInsensitive) ||
                               fileName.startsWith("mmst://", Qt::CaseInsensitive) ||
                               fileName.startsWith("rtp://", Qt::CaseInsensitive) ||
                               fileName.startsWith("sdp://", Qt::CaseInsensitive);

        return isNetworkStream;
}

} // namespace GuiUtils
