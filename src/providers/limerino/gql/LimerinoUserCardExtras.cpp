// SPDX-License-Identifier: MIT

#include "providers/limerino/gql/LimerinoUserCardExtras.hpp"

#include "common/QLogging.hpp"
#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/gql/LimerinoGql.hpp"

#include <QDateTime>
#include <QLoggingCategory>

namespace chatterino::LimerinoAuth::gql {

namespace {

constexpr qint64 CACHE_TTL_MS = 5 * 60 * 1000;  // 5 min (ruling 5)

// Authored doc combining the three field paths (ruling 6). Verified against:
//   gqlreference/gql/fragments.js      (settings.preferredLanguageTag, primaryTeam)
//   gqlreference/gql/user/queries.js   (GET_RELATIONSHIP subscriptionBenefit block)
inline const QString USER_CARD_EXTRAS_QUERY = QStringLiteral(R"GQL(
query LimerinoUserCardExtras($id: ID!, $channelID: ID!) {
    user(id: $id) {
        settings { preferredLanguageTag }
        primaryTeam { name owner { login } }
        relationship(targetUserID: $channelID) {
            subscriptionTenure(tenureMethod: CUMULATIVE) { months }
            subscriptionBenefit { platform purchasedWithPrime tier thirdPartySKU gift { isGift } }
        }
    }
}
)GQL");

struct CacheEntry {
    QString channelId;
    QDateTime storedAt;
    LimerinoUserCardExtras extras;
};

QHash<QString, CacheEntry> &extrasCache()
{
    static QHash<QString, CacheEntry> cache;
    return cache;
}

}  // namespace

LimerinoUserCardExtras parseUserCardExtras(const QJsonObject &userObj,
                                           bool settingsFailed,
                                           bool relationshipFailed)
{
    LimerinoUserCardExtras out;

    if (!settingsFailed)
    {
        // settings may be null (permissions) -> toObject() yields {} -> empty.
        out.preferredLanguageTag =
            userObj.value(QStringLiteral("settings"))
                .toObject()
                .value(QStringLiteral("preferredLanguageTag"))
                .toString();
    }

    const QJsonObject team =
        userObj.value(QStringLiteral("primaryTeam")).toObject();
    out.primaryTeamName =
        team.value(QStringLiteral("name")).toString();  // empty when absent

    if (!relationshipFailed)
    {
        const QJsonObject rel =
            userObj.value(QStringLiteral("relationship")).toObject();
        const QJsonObject sub =
            rel.value(QStringLiteral("subscriptionBenefit")).toObject();
        if (!sub.isEmpty())
        {
            LimerinoSubDetail det;
            det.platform = sub.value(QStringLiteral("platform")).toString();
            det.purchasedWithPrime =
                sub.value(QStringLiteral("purchasedWithPrime")).toBool();
            det.tier = sub.value(QStringLiteral("tier")).toString();
            det.isGift = sub.value(QStringLiteral("gift"))
                             .toObject()
                             .value(QStringLiteral("isGift"))
                             .toBool();
            det.thirdPartySKU =
                sub.value(QStringLiteral("thirdPartySKU")).toString();
            det.tenureMonths =
                rel.value(QStringLiteral("subscriptionTenure"))
                    .toObject()
                    .value(QStringLiteral("months"))
                    .toInt();
            out.subscription = det;
        }
    }

    return out;
}

void fetchUserCardExtras(
    const QString &targetUserId, const QString &channelId,
    std::function<void(std::optional<LimerinoUserCardExtras>)> cb)
{
    if (targetUserId.isEmpty())
    {
        if (cb)
        {
            cb(std::nullopt);
        }
        return;
    }

    auto &cache = extrasCache();
    auto it = cache.find(targetUserId);
    if (it != cache.end() && it->channelId == channelId &&
        it->storedAt.msecsTo(QDateTime::currentDateTimeUtc()) < CACHE_TTL_MS)
    {
        if (cb)
        {
            cb(it->extras);
        }
        return;
    }

    QString resolveErr;
    auto token = LimerinoAuth::resolveReadToken(&resolveErr);
    if (!token.hasToken())
    {
        qCDebug(chatterinoLiveupdates)
            << "UserCardExtras: no token for" << targetUserId
            << "-" << resolveErr;
        if (cb)
        {
            cb(std::nullopt);
        }
        return;
    }

    QJsonObject variables;
    variables.insert(QStringLiteral("id"), targetUserId);
    variables.insert(QStringLiteral("channelID"), channelId);

    executeInline(
        QStringLiteral("LimerinoUserCardExtras"), USER_CARD_EXTRAS_QUERY,
        variables, token.token,
        [targetUserId, channelId, cb](const QJsonObject &data) {
            const QJsonObject user =
                data.value(QStringLiteral("user")).toObject();
            if (user.isEmpty())
            {
                qCDebug(chatterinoLiveupdates)
                    << "UserCardExtras: no user object for" << targetUserId;
                if (cb)
                {
                    cb(std::nullopt);
                }
                return;  // not cached: transient or deleted user
            }

            const auto extras = parseUserCardExtras(
                user, /*settingsFailed*/ false, /*relationshipFailed*/ false);

            // A response that carries a user but *no* extras at all is still a
            // perfectly valid "nothing to show" - cached so repeat opens are free.
            extrasCache().insert(targetUserId,
                                 CacheEntry{channelId,
                                            QDateTime::currentDateTimeUtc(),
                                            extras});

            if (cb)
            {
                cb(extras);
            }
        },
        [targetUserId, cb](const GqlError &err) {
            qCDebug(chatterinoLiveupdates)
                << "UserCardExtras failed for" << targetUserId << ":"
                << (err.httpFailed ? QString::number(err.httpStatus)
                                   : err.message);
            if (cb)
            {
                cb(std::nullopt);
            }
        });
}

}  // namespace chatterino::LimerinoAuth::gql
