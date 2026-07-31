// SPDX-License-Identifier: MIT
// Secondary "Limerino Extra Features" auth.
// Completely separate store from the primary Twitch login (/accounts/uid<id>/).
// See FORK.md.

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <pajlada/signals/signal.hpp>

#include <functional>

namespace chatterino::LimerinoAuth {

struct LimerinoAuthChannel {
    QString id;
    QString login;
    QString displayName;

    bool operator==(const LimerinoAuthChannel &other) const
    {
        return this->id == other.id && this->login == other.login &&
               this->displayName == other.displayName;
    }
};

struct LimerinoAuthAccount {
    QString userId;
    QString login;
    QString displayName;
    QString token;
    bool valid = false;
    QString lastError;
    QDateTime lastValidatedAt;
    QVector<LimerinoAuthChannel> moderatedChannels;
};

struct LimerinoAuthToken {
    QString token;
    QString userId;
    QString login;
    bool fromScript = false;

    bool hasToken() const
    {
        return !this->token.isEmpty();
    }
};

struct LimerinoAuthSummary {
    int accountCount = 0;
    int validAccountCount = 0;
    int invalidAccountCount = 0;
    int moderatedChannelCount = 0;
};

struct LimerinoAuthRefreshResult {
    int total = 0;
    int valid = 0;
    int invalid = 0;
    int moderatedChannels = 0;
    QStringList errors;
};

// Trim, strip wrapping quotes and common header prefixes, loop until stable.
// Users paste raw header values; this makes pasting "Authorization: Bearer xyz" safe.
QString normalizeToken(QString raw);

QVector<LimerinoAuthAccount> accounts();
LimerinoAuthSummary summary();

// Token must already be normalized. Validation happens asynchronously;
// the account appears (possibly unvalidated) immediately via accountsChanged.
void addOrUpdateToken(const LimerinoAuthToken &token);
void removeAccount(const QString &userId);

// Re-validates every stored account. `done` runs once, on the GUI thread.
void refreshAccounts(
    const std::function<void(const LimerinoAuthRefreshResult &)> &done = {});

// One-shot deferred refresh, safe to call from app startup. Runs at most once.
void scheduleStartupRefresh();

// Emitted whenever the account store changes (add/remove/update/validate).
extern pajlada::Signals::Signal<void()> accountsChanged;

// --- exposed for the device login flow (Phase 2) and feature resolver (Phase 5) ---

// Asynchronously validates `normalizedToken` against Twitch and produces a fully
// resolved account (valid flag, identity, display name, moderated channels).
void resolveToken(
    const QString &normalizedToken, bool fromScript,
    const std::function<void(const LimerinoAuthAccount &)> &onDone);

// Device flow constants (Phase 2). Values come from the frozen auth artifact
// (resources/limerino/limerinoauth.txt); the client id is identical to the
// primary login's public one.
extern const QString CLIENT_ID;
extern const QString CLIENT_SCOPES;
extern const QString AUTH_DEVICE_URL;
extern const QString AUTH_TOKEN_URL;

}  // namespace chatterino::LimerinoAuth
