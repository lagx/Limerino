// SPDX-License-Identifier: MIT
// Theme creator dialog (batch T3).
//
// Four seed colors in, live preview via Theme::setAutoReload + rewriting a
// working Themes/_LimerinoPreview.json file. ColorPickerDialog for picking;
// QToolButton swatch + editable hex for each row. No parallel preview path.

#pragma once

#include "providers/limerino/theme/LimerinoThemeSeed.hpp"
#include "widgets/BasePopup.hpp"

#include <QString>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QToolButton;

namespace chatterino::limerino {

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
    void setSwatchColor(QToolButton *swatch, const QColor &color);
    void setFieldColor(SeedField field, const QColor &color, bool updateHex);
    QColor &fieldColor(SeedField field);
    const QColor &fieldColor(SeedField field) const;

    void openColorPicker(SeedField field);
    void onHexEdited(SeedField field);
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
    LimerinoThemeSeed seedBeforePicker_{};
    bool pickerConfirmed_ = false;
    bool applied_ = false;
    bool previewActive_ = false;
    QString previousThemeName_;

    QLineEdit *nameEdit_ = nullptr;

    QToolButton *backgroundSwatch_ = nullptr;
    QLineEdit *backgroundHex_ = nullptr;
    QToolButton *surfaceSwatch_ = nullptr;
    QLineEdit *surfaceHex_ = nullptr;
    QToolButton *accentSwatch_ = nullptr;
    QLineEdit *accentHex_ = nullptr;
    QToolButton *textSwatch_ = nullptr;
    QLineEdit *textHex_ = nullptr;

    QLabel *warningsLabel_ = nullptr;
};

}  // namespace chatterino::limerino
