// SPDX-License-Identifier: MIT
// Crossban channel presets. JSON under /limerino/crossban/presets.
// Default empty — user builds presets in the dialog.

#pragma once

#include <QString>
#include <QUuid>
#include <QVector>

namespace chatterino::limerino {

struct CrossbanChannel {
    QString id;
    QString login;
    QString displayName;
};

struct CrossbanPreset {
    QUuid id;
    QString name;
    QVector<CrossbanChannel> channels;
};

QVector<CrossbanPreset> loadCrossbanPresets();
void saveCrossbanPresets(const QVector<CrossbanPreset> &presets);

QUuid lastCrossbanPresetId();
void setLastCrossbanPresetId(const QUuid &id);

}  // namespace chatterino::limerino
