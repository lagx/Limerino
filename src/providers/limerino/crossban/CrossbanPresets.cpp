// SPDX-License-Identifier: MIT

#include "providers/limerino/crossban/CrossbanPresets.hpp"

#include "singletons/Settings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace chatterino::limerino {

namespace {

CrossbanChannel channelFromJson(const QJsonObject &o)
{
    return CrossbanChannel{
        o[QStringLiteral("id")].toString(),
        o[QStringLiteral("login")].toString(),
        o[QStringLiteral("displayName")].toString(),
    };
}

QJsonObject channelToJson(const CrossbanChannel &c)
{
    return QJsonObject{
        {QStringLiteral("id"), c.id},
        {QStringLiteral("login"), c.login},
        {QStringLiteral("displayName"), c.displayName},
    };
}

CrossbanPreset presetFromJson(const QJsonObject &o)
{
    CrossbanPreset p;
    p.id = QUuid::fromString(o[QStringLiteral("id")].toString());
    if (p.id.isNull())
    {
        p.id = QUuid::createUuid();
    }
    p.name = o[QStringLiteral("name")].toString().trimmed();
    for (const auto &v : o[QStringLiteral("channels")].toArray())
    {
        if (!v.isObject())
        {
            continue;
        }
        auto c = channelFromJson(v.toObject());
        if (c.id.isEmpty() && c.login.isEmpty())
        {
            continue;
        }
        p.channels.append(std::move(c));
    }
    return p;
}

QJsonObject presetToJson(const CrossbanPreset &p)
{
    QJsonArray channels;
    for (const auto &c : p.channels)
    {
        channels.append(channelToJson(c));
    }
    return QJsonObject{
        {QStringLiteral("id"), p.id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), p.name},
        {QStringLiteral("channels"), channels},
    };
}

}  // namespace

QVector<CrossbanPreset> loadCrossbanPresets()
{
    const auto raw = getSettings()->limerinoCrossbanPresets.getValue();
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
    {
        return {};
    }
    QVector<CrossbanPreset> out;
    for (const auto &v : doc.array())
    {
        if (!v.isObject())
        {
            continue;
        }
        auto p = presetFromJson(v.toObject());
        if (p.name.isEmpty())
        {
            continue;
        }
        out.append(std::move(p));
    }
    return out;
}

void saveCrossbanPresets(const QVector<CrossbanPreset> &presets)
{
    QJsonArray arr;
    for (const auto &p : presets)
    {
        arr.append(presetToJson(p));
    }
    getSettings()->limerinoCrossbanPresets.setValue(
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QUuid lastCrossbanPresetId()
{
    return QUuid::fromString(
        getSettings()->limerinoCrossbanLastPresetId.getValue());
}

void setLastCrossbanPresetId(const QUuid &id)
{
    getSettings()->limerinoCrossbanLastPresetId.setValue(
        id.isNull() ? QString() : id.toString(QUuid::WithoutBraces));
}

}  // namespace chatterino::limerino
