// SPDX-License-Identifier: MIT
// Limerino theme creator: pure generator, seed -> QJsonObject. See the .cpp
// for the per-leaf derivation table.

#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace chatterino::limerino {

struct LimerinoThemeSeed;

/// Generate a complete theme JSON from the seed. Pure; emits every leaf the
/// parser knows about (there is no partial-theme risk).
QJsonObject generateTheme(const LimerinoThemeSeed &seed);

/// Pairs the generator can't make readable at low contrast, one line per
/// violation. Empty list = no warnings. Does not mutate the output.
QStringList contrastWarnings(const LimerinoThemeSeed &seed);

}  // namespace chatterino::limerino
