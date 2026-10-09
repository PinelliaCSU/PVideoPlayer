#include "settingsrepository.h"

#include <QDir>

QSettingsRepository::QSettingsRepository(const QString &filePath)
    : m_filePath(filePath)
{
    const QFileInfo fileInfo(m_filePath);
    QDir().mkpath(fileInfo.absolutePath());
}

QVariant QSettingsRepository::value(const QString &key, const QVariant &defaultValue) const
{
    QSettings settings(m_filePath, QSettings::IniFormat);
    return settings.value(key, defaultValue);
}

void QSettingsRepository::setValue(const QString &key, const QVariant &value)
{
    QSettings settings(m_filePath, QSettings::IniFormat);
    settings.setValue(key, value);
    settings.sync();
}

void QSettingsRepository::remove(const QString &key)
{
    QSettings settings(m_filePath, QSettings::IniFormat);
    settings.remove(key);
    settings.sync();
}
