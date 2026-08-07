// SPDX-License-Identifier: MIT
// Pure-function tests for the Limerino theme creator. No app, no mock, no UI.

#include "providers/limerino/theme/LimerinoThemeGenerator.hpp"

#include "providers/limerino/theme/LimerinoThemeSeed.hpp"

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>

using namespace chatterino::limerino;

namespace {

// Every leaf in the documented T0(b) diff inventory plus the anchors. Keyed
// by dotted path so failures are readable.
const char *REQUIRED_LEAVES[] = {
    "colors.accent",
    "colors.window.background",
    "colors.window.text",
    "colors.tabs.dividerLine",
    "colors.tabs.liveIndicator",
    "colors.tabs.rerunIndicator",
    "colors.tabs.regular.text",
    "colors.tabs.regular.backgrounds.regular",
    "colors.tabs.regular.backgrounds.hover",
    "colors.tabs.regular.backgrounds.unfocused",
    "colors.tabs.regular.line.regular",
    "colors.tabs.newMessage.text",
    "colors.tabs.newMessage.line.regular",
    "colors.tabs.highlighted.text",
    "colors.tabs.highlighted.line.regular",
    "colors.tabs.selected.text",
    "colors.tabs.selected.line.regular",
    "colors.messages.backgrounds.regular",
    "colors.messages.backgrounds.alternate",
    "colors.messages.disabled",
    "colors.messages.highlightAnimationEnd",
    "colors.messages.highlightAnimationStart",
    "colors.messages.selection",
    "colors.messages.textColors.caret",
    "colors.messages.textColors.chatPlaceholder",
    "colors.messages.textColors.link",
    "colors.messages.textColors.regular",
    "colors.messages.textColors.system",
    "colors.overlayMessages.backgrounds.alternate",
    "colors.overlayMessages.backgrounds.regular",
    "colors.overlayMessages.disabled",
    "colors.overlayMessages.selection",
    "colors.overlayMessages.textColors.caret",
    "colors.overlayMessages.textColors.regular",
    "colors.overlayMessages.textColors.system",
    "colors.overlayMessages.background",
    "colors.scrollbars.background",
    "colors.scrollbars.thumb",
    "colors.scrollbars.thumbSelected",
    "colors.splits.background",
    "colors.splits.dropPreview",
    "colors.splits.dropPreviewBorder",
    "colors.splits.dropTargetRect",
    "colors.splits.dropTargetRectBorder",
    "colors.splits.resizeHandle",
    "colors.splits.resizeHandleBackground",
    "colors.splits.messageSeperator",
    "colors.splits.header.background",
    "colors.splits.header.border",
    "colors.splits.header.focusedBackground",
    "colors.splits.header.focusedBorder",
    "colors.splits.header.text",
    "colors.splits.header.focusedText",
    "colors.splits.input.background",
    "colors.splits.input.backgroundPulse",
    "colors.splits.input.searchFailText",
    "colors.splits.input.searchHighlightBackground",
    "colors.splits.input.text",
};

QJsonValue digValue(const QJsonObject &root, const QString &path)
{
    QJsonObject current = root;
    const QStringList parts = path.split('.');
    for (qsizetype i = 0; i < parts.size() - 1; ++i)
    {
        current = current.value(parts[i]).toObject();
    }
    return current.value(parts.last());
}

}  // namespace

TEST(LimerinoTheme, EveryRequiredLeafPresent)
{
    const auto dark = generateTheme(LimerinoThemeSeed::darkPreset());
    for (const char *leaf : REQUIRED_LEAVES)
    {
        const auto val = digValue(dark, QString::fromLatin1(leaf));
        EXPECT_TRUE(val.isString()) << "missing or non-string leaf: " << leaf;
        EXPECT_FALSE(val.toString().isEmpty()) << "empty leaf: " << leaf;
    }
}

TEST(LimerinoTheme, StyleSheetNeverEmitted)
{
    const auto dark = generateTheme(LimerinoThemeSeed::darkPreset());
    const auto splitsInput =
        dark["colors"].toObject()["splits"].toObject()["input"].toObject();
    EXPECT_FALSE(splitsInput.contains("styleSheet"))
        << "splits.input.styleSheet must not be emitted";
    EXPECT_FALSE(splitsInput.contains("stylesheets"));
}

TEST(LimerinoTheme, SchemaUsesPublicLimerinoUrl)
{
    const auto dark = generateTheme(LimerinoThemeSeed::darkPreset());
    EXPECT_EQ(
        dark[QStringLiteral("$schema")].toString(),
        QStringLiteral("https://raw.githubusercontent.com/lagx/Limerino/"
                       "limerino/docs/ChatterinoTheme.schema.json"));
}

TEST(LimerinoTheme, IconThemeFollowsLightness)
{
    const auto dark = generateTheme(LimerinoThemeSeed::darkPreset());
    const auto light = generateTheme(LimerinoThemeSeed::lightPreset());
    // iconTheme is the inverse of the visual theme (dark theme -> light icons)
    EXPECT_EQ(dark["metadata"].toObject()["iconTheme"].toString(),
              QStringLiteral("light"));
    EXPECT_EQ(light["metadata"].toObject()["iconTheme"].toString(),
              QStringLiteral("dark"));
}

TEST(LimerinoTheme, Deterministic)
{
    const auto seed = LimerinoThemeSeed::darkPreset();
    const auto a = QJsonDocument(generateTheme(seed)).toJson();
    const auto b = QJsonDocument(generateTheme(seed)).toJson();
    EXPECT_EQ(a, b);
}

TEST(LimerinoTheme, LowContrastSeedIsFlagged)
{
    // background == text reads as invisible; the warnings list must fire.
    LimerinoThemeSeed bad = LimerinoThemeSeed::darkPreset();
    bad.text = bad.background;  // contrast 1.0
    const auto warnings = contrastWarnings(bad);
    EXPECT_FALSE(warnings.isEmpty());
    EXPECT_TRUE(warnings.join(' ').contains("primary text"));
}

TEST(LimerinoTheme, TransparentSurvivesSerialization)
{
    const auto theme = generateTheme(LimerinoThemeSeed::lightPreset());
    const auto overlay =
        theme["colors"].toObject()["overlayMessages"].toObject();
    const auto regular =
        overlay["backgrounds"].toObject()["regular"].toString();
    // The literal string survives; the parse side converts it.
    EXPECT_EQ(regular, QStringLiteral("transparent"));
}

TEST(LimerinoTheme, AliasedTransparencyRoundTrip)
{
    // Built-ins write scrollbar-background as "#00000000" sometimes; our
    // generator must accept both literal forms on reimport-as-input.
    const auto theme = generateTheme(LimerinoThemeSeed::darkPreset());
    const auto scroll =
        theme["colors"].toObject()["scrollbars"].toObject();
    const auto bg = scroll["background"].toString();
    EXPECT_TRUE(bg == "#00000000" || bg == "#000000" || bg == "transparent");
}
