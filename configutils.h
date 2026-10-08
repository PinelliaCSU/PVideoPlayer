#ifndef CONFIGUTILS_H
#define CONFIGUTILS_H


#include <QString>
#include <QStringList>
#include <QDateTime>

namespace ConfigUtils
{
// 从本地文件配置文件获取音量
void LoadVolume(double& volume);
// 设置音量同步到本地文件配置文件中
void SaveVolume(double& volume);

// 获取播放列表
void LoadPlaylist(QStringList& playList);
// 保存播放列表
void SavePlaylist(QStringList& playList);


// 保存/读取播放位置（秒）
void SavePlaybackPos(const QString& filePath, int seconds);
int  LoadPlaybackPos(const QString& filePath);
void ClearPlaybackPos(const QString& filePath);

// ========== 播放历史 ==========
struct PlayHistoryItem {
    QString   filePath;      // 文件完整路径或 URL
    int       lastPosition;  // 上次播放位置（秒）
    int       totalDuration; // 总时长（秒）
    QDateTime lastPlayed;    // 最后播放时间
};
void SavePlayHistory(const QList<PlayHistoryItem>& history);
QList<PlayHistoryItem> LoadPlayHistory();


const int MAX_SLIDER_VALUE = 65536;
} // namespace Config



#endif // CONFIGUTILS_H
