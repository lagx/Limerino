// SPDX-License-Identifier: MIT
// Theme creator dialog (batch T3).
//
// Four seed colors in, live preview via Theme::setAutoReload + rewriting a
// working Themes/_LimerinoPreview.json file. Colour rows use shared
// LimerinoColorField (ColorPickerDialog + swatch + hex). No parallel preview.

#pragma once

#include "providers/limerino/theme/LimerinoThemeSeed.hpp"
#include "widgets/BasePopup.hpp"

#include <QString>

class QCloseEvent;
class QLabel;
class QLineEdit;

namespace chatterino::limerino {

class LimerinoColorField;

class LimerinoThemeDialog : public BasePopup
{
    Q_OBJECT

public:
    explicit LimerinoThemeDialog(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class SeedField {
        Background,
        Surface,
        Accent,
        Text,
    };

    void buildUi();
    void syncUiFromSeed();
    void setFieldColor(SeedField field, const QColor &color);
    QColor &fieldColor(SeedField field);
    const QColor &fieldColor(SeedField field) const;

    void refreshWarnings();
    void writePreviewAndReload();
    void startLivePreview();
    void teardownPreview(bool restorePreviousTheme);

    void onDarkPreset();
    void onLightPreset();
    void onImport();
    void onExportSeed();
    void onExportTheme();
    void onApply();
    void onCancel();

    bool isReservedThemeFilename(const QString &filename) const;
    QString previewFilePath() const;

    LimerinoThemeSeed seed_ = LimerinoThemeSeed::darkPreset();
    bool applied_ = false;
    bool previewActive_ = false;
    QString previousThemeName_;

    QLineEdit *nameEdit_ = nullptr;

    LimerinoColorField *backgroundField_ = nullptr;
    LimerinoColorField *surfaceField_ = nullptr;
    LimerinoColorField *accentField_ = nullptr;
    LimerinoColorField *textField_ = nullptr;

    QLabel *warningsLabel_ = nullptr;
};

}  // namespace chatterino::limerino
