// SPDX-License-Identifier: MIT
// Batch 1 (Identity & account): /uid, /namehistory, /modlist ports plus the
// usercard "Name history" entry. Bodies transcribed from
// pluginforreference/init.lua + requests.lua.

#pragma once

#include <QString>
#include <QVector>

class QWidget;

namespace chatterino {

struct CommandContext;

namespace LimerinoCommands {

QString uid(const CommandContext &ctx);
QString nameHistory(const CommandContext &ctx);
QString modList(const CommandContext &ctx);

// Usercard integration (UserInfoPopup hook).
void showNameHistoryDialog(const QString &login, QWidget *parent);

// Drives the Commands wiki at the top of the Limerino settings page
// (registry-generated; extended by every batch - never a hardcoded list).
struct CommandDoc {
    QString names;
    QString usage;
    QString description;
};
QVector<CommandDoc> commandDocs();

}  // namespace LimerinoCommands
}  // namespace chatterino
