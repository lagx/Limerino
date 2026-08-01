// SPDX-License-Identifier: MIT
// Dialog for the secondary "Limerino Extra Features" auth.
// Deliberately a NEW dialog; the stock LoginDialog is untouched.

#pragma once

#include "providers/limerino/LimerinoAuth.hpp"
#include "widgets/BasePopup.hpp"

#include <QPointer>

#include <pajlada/signals/signalholder.hpp>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTextEdit;

namespace chatterino {

class LimerinoAuthDialog final : public BasePopup
{
    Q_OBJECT

public:
    explicit LimerinoAuthDialog(QWidget *parent = nullptr);

private:
    void rebuildAccountsTable();
    void setDeviceStatusText(
        const LimerinoAuth::DeviceLogin::Status &status);

    struct {
        QTabWidget *tabs = nullptr;

        QLabel *deviceStatus = nullptr;
        QLabel *deviceCode = nullptr;
        QPushButton *deviceStart = nullptr;
        QPushButton *deviceCancel = nullptr;

        QLineEdit *scriptTokenInput = nullptr;

        QTableWidget *accountsTable = nullptr;
        QLabel *accountsSummary = nullptr;
    } ui_;

    QPointer<LimerinoAuth::DeviceLogin> deviceLogin_;
    pajlada::Signals::SignalHolder managedConnections_;
};

}  // namespace chatterino
