#ifndef TST_SCREENSHOTUTILS_H
#define TST_SCREENSHOTUTILS_H

#include <QObject>

// 截图文件名与保存路径规则的无 GUI 测试
class ScreenshotUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void fileNameContainsMediaNameAndTimestamp();
    void fileNameStripsDirectoryAndExtension();
    void fileNameSanitizesIllegalCharacters();
    void fileNameFallsBackForNetworkStream();
    void uniquePathAvoidsOverwriting();
    void defaultDirectoryIsNotEmpty();
};

#endif // TST_SCREENSHOTUTILS_H
