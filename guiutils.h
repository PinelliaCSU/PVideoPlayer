#ifndef GUIUTILS_H
#define GUIUTILS_H

#include <QString>
#include <QPushButton>

namespace GuiUtils
{
// 获取Qss样式
QString LoadQss(const QString& qssPath);

// 为按钮设置字体图标
void SetIcon(QPushButton *btn, int iconSize, QChar icon);

// 将按键代码转换为可读的按键名称
QString GetKeyName(int key);

bool CheckNetworkStream(const QString& fileName);
} // namespace GuiUtils

#endif // GUIUTILS_H
