#ifndef SETTINGSREPOSITORY_H
#define SETTINGSREPOSITORY_H

#include <QSettings>
#include <QString>
#include <QVariant>

class ISettingsRepository
{
public:
    virtual ~ISettingsRepository() = default;
    virtual QVariant value(const QString &key, const QVariant &defaultValue = {}) const = 0;
    virtual void setValue(const QString &key, const QVariant &value) = 0;
    virtual void remove(const QString &key) = 0;
};

class QSettingsRepository final : public ISettingsRepository
{
public:
    explicit QSettingsRepository(const QString &filePath);

    QVariant value(const QString &key, const QVariant &defaultValue = {}) const override;
    void setValue(const QString &key, const QVariant &value) override;
    void remove(const QString &key) override;

private:
    QString m_filePath;
};

#endif // SETTINGSREPOSITORY_H
