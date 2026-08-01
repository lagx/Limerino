// SPDX-License-Identifier: MIT
// Channel-points prediction manager window (batch 4).
// - Create form: title, duration (seconds), 2-10 options with + / x buttons
// - History: last 5 predictions created through this window (click to reuse)
// - Active prediction: lock it or pay out on an outcome
// GQL ops transcribed from pluginforreference/requests.lua.

#pragma once

#include "widgets/BasePopup.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

namespace chatterino {
class Split;
class TwitchChannel;

namespace limerino {

class LimerinoPredictionDialog final : public BasePopup
{
    Q_OBJECT

public:
    explicit LimerinoPredictionDialog(Split *split);

private:
    void refreshContext();
    void submitCreate();
    void addOptionRow(const QString &text = QString());
    void appendHistory(const QString &title, const QStringList &options,
                       int windowSeconds);
    void refillFromHistory(int index);
    void lockPrediction(const QString &eventId);
    void payoutPrediction(const QString &eventId, const QString &outcomeId);

    Split *split_ = nullptr;

    QLineEdit *titleEdit_ = nullptr;
    QSpinBox *windowSpin_ = nullptr;
    QVBoxLayout *optionsLayout_ = nullptr;
    QPushButton *addOptionButton_ = nullptr;
    QPushButton *createButton_ = nullptr;

    QLabel *historyTitle_ = nullptr;
    QComboBox *historyBox_ = nullptr;
    QPushButton *historyUseButton_ = nullptr;

    QLabel *activeLabel_ = nullptr;
    QVBoxLayout *activeLayout_ = nullptr;
    QPushButton *lockButton_ = nullptr;
    QPushButton *refreshButton_ = nullptr;

    QString activeEventId_;
};

}  // namespace limerino
}  // namespace chatterino
