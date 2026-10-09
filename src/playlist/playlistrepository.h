#ifndef PLAYLISTREPOSITORY_H
#define PLAYLISTREPOSITORY_H

#include <QStringList>

/*
 * 播放列表持久化仓储接口：播放列表控件和模型都不直接读写配置文件，
 * 由应用控制器通过该接口完成加载和保存，测试可替换为内存实现。
 */
class IPlaylistRepository
{
public:
    virtual ~IPlaylistRepository() = default;

    virtual QStringList load() const = 0;
    virtual void save(const QStringList &locators) const = 0;
};

// 默认实现：写入应用配置文件（player_config.ini）
class SettingsPlaylistRepository final : public IPlaylistRepository
{
public:
    QStringList load() const override;
    void save(const QStringList &locators) const override;
};

#endif // PLAYLISTREPOSITORY_H
