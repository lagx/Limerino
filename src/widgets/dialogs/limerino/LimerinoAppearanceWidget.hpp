// SPDX-License-Identifier: MIT
// Appearance page of the channel-points window: pick global badge, chat color.
// - available badges: ChatSettings_Badges (persisted op, channel context)
// - apply badge: ChatSettings_SelectGlobalBadge (persisted op, plugin fields verbatim)
// - chat color: native `updateUserChatColor` (same call the stock /color command runs)
// Per-channel badge mutation doesn't exist in any verified API; the scope
// switch shows that plainly instead of pretending.

#pragma once

#include "widgets/BaseWidget.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace chatterino {
class Split;
class TwitchChannel;

namespace limerino {

class LimerinoAppearanceWidget final : public BaseWidget
{
    Q_OBJECT

public:
    explicit LimerinoAppearanceWidget(Split *split);

private:
    void refreshBadges();
    void refreshPreview();
    void applyBadge();
    void applyColor();

    Split *split_ = nullptr;

    QComboBox *badgeCombo_ = nullptr;
    QComboBox *scopeCombo_ = nullptr;
    QLabel *previewLabel_ = nullptr;

    QComboBox *colorCombo_ = nullptr;
    QLineEdit *colorHexEdit_ = nullptr;
    QLabel *colorPreviewLabel_ = nullptr;
    QPushButton *applyColorButton_ = nullptr;

    QPushButton *applyBadgeButton_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};

}  // namespace limerino
}  // namespace chatterino
