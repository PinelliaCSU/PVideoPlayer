#include "configutils.h"
#include <QSettings>
#include <QDir>

namespace {
// 将常量定义在匿名命名空间中，使其只在当前文件可见
const  QString PLAYER_CONFIG_FILENAME = "player_config.ini";
const  QString PLAYER_CONFIG_FILEPATH = QDir::tempPath() + QDir::separator() + PLAYER_CONFIG_FILENAME;
}

namespace ConfigUtils
{
void LoadVolume(double& volume)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    volume = settings.value("volume/size", 0.5).toDouble(); // 提供一个默认值
}

void SaveVolume(double& volume)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    settings.setValue("volume/size", volume);
}

void LoadPlaylist(QStringList& playList)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    playList = settings.value("playlist/files").toStringList();
}

void SavePlaylist(QStringList& playList)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    settings.setValue("playlist/files", playList);
}

void SavePlaybackPos(const QString& filePath, int seconds)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    // 使用文件路径的 hash 作为 key 前缀，避免路径中的特殊字符干扰 QSettings 的 key 解析
    QString key = QString("playback_pos/%1").arg(QString::number(qHash(filePath), 16));
    settings.setValue(key + "/path", filePath);
    settings.setValue(key + "/pos", seconds);
}

int LoadPlaybackPos(const QString& filePath)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    QString key = QString("playback_pos/%1").arg(QString::number(qHash(filePath), 16));
    QString savedPath = settings.value(key + "/path").toString();
    // 验证路径匹配（防止 hash 碰撞）
    if (savedPath == filePath) {
        return settings.value(key + "/pos", -1).toInt();
    }
    return -1;
}

void ClearPlaybackPos(const QString& filePath)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    QString key = QString("playback_pos/%1").arg(QString::number(qHash(filePath), 16));
    settings.remove(key);
}

// ========== 播放历史 ==========

void SavePlayHistory(const QList<PlayHistoryItem>& history)
{
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    settings.beginGroup("play_history");
    settings.remove("");  // 清空旧数据
    settings.beginWriteArray("items", history.size());
    for (int i = 0; i < history.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", history[i].filePath);
        settings.setValue("pos", history[i].lastPosition);
        settings.setValue("dur", history[i].totalDuration);
        settings.setValue("time", history[i].lastPlayed);
    }
    settings.endArray();
    settings.endGroup();
}

QList<PlayHistoryItem> LoadPlayHistory()
{
    QList<PlayHistoryItem> history;
    QSettings settings(PLAYER_CONFIG_FILEPATH, QSettings::IniFormat);
    settings.beginGroup("play_history");
    int size = settings.beginReadArray("items");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        PlayHistoryItem item;
        item.filePath      = settings.value("path").toString();
        item.lastPosition  = settings.value("pos", 0).toInt();
        item.totalDuration = settings.value("dur", 0).toInt();
        item.lastPlayed    = settings.value("time").toDateTime();
        if (!item.filePath.isEmpty()) {
            history.append(item);
        }
    }
    settings.endArray();
    settings.endGroup();
    return history;
}


} // namespace Config
