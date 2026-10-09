#include "playlistrepository.h"

#include "configutils.h"

QStringList SettingsPlaylistRepository::load() const
{
    QStringList locators;
    ConfigUtils::LoadPlaylist(locators);
    return locators;
}

void SettingsPlaylistRepository::save(const QStringList &locators) const
{
    QStringList mutableLocators = locators;
    ConfigUtils::SavePlaylist(mutableLocators);
}
