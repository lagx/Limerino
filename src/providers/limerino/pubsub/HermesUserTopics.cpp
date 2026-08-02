// SPDX-License-Identifier: MIT

#include "providers/limerino/pubsub/HermesUserTopics.hpp"

#include "Application.hpp"
#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/commands/Identity.hpp"
#include "providers/limerino/gql/LimerinoGql.hpp"
#include "providers/limerino/gql/PersistedQueries.hpp"
#include "providers/limerino/pubsub/LimerinoPubSubController.hpp"
#include "providers/limerino/pubsub/LimerinoPubSubTopics.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <array>
#include <unordered_map>

namespace chatterino::limerino {

namespace {

// chatrooms-user-v1 reference cooldown (events.js L14). Kept channel-local,
// so a warn burst in one channel doesn't suppress the next channel's.
constexpr int USER_MOD_ACTION_COOLDOWN_MS = 2500;

struct WarnCooldown {
    qint64 issuedAtMs = 0;
};
std::unordered_map<QString, WarnCooldown> &warnCooldowns()
{
    static auto *map = new std::unordered_map<QString, WarnCooldown>;
    return *map;
}

bool warnRecentlyIn(const QString &channelId)
{
    auto &map = warnCooldowns();
    const auto now = QDateTime::currentMSecsSinceEpoch();
    auto it = map.find(channelId);
    if (it != map.end() && now - it->second.issuedAtMs < USER_MOD_ACTION_COOLDOWN_MS)
    {
        return true;
    }
    map[channelId] = WarnCooldown{now};
    return false;
}

// Resolve a display name best-effort for chat reads. Priority = displayName,
// then login. channelId -> name lookup is done via the live Twitch channel
// (where that's open); otherwise we just print the id.
QString describeChannel(const QString &channelId)
{
    auto channelPtr =
        getApp()->getTwitch()->getChannelOrEmptyByID(channelId);
    if (channelPtr->isEmpty())
    {
        return channelId;
    }

    auto *tchan = dynamic_cast<TwitchChannel *>(channelPtr.get());
    if (tchan == nullptr)
    {
        return channelId;
    }
    return tchan->getName();  // TwitchChannel::getName is login (or #login)
}

void acknowledgeWarningFor(const QString &channelId)
{
    if (!getSettings()->limerinoAutoAcknowledgeChatWarnings.getValue())
    {
        return;
    }

    // Same mutation as /acknowledgewarning; from PersistedQueries.
    const QString gqlToken = [&]() {
        QString err;
        auto t = LimerinoAuth::resolveCurrentUserToken(&err);
        return t.hasToken() ? t.token : QString();
    }();
    if (gqlToken.isEmpty())
    {
        return;  // silently skip: the warning line already says why
    }

    LimerinoAuth::gql::executePersisted(
        LimerinoAuth::gql::PQ_ACKNOWLEDGE_CHAT_WARNING,
        QJsonObject{{QStringLiteral("input"),
                     QJsonObject{{QStringLiteral("channelID"), channelId}}}},
        gqlToken, [](const QJsonObject & /*data*/) {},
        [](const auto & /*error*/) {});
}

// Topic-allowed actions (specstring: warn + acknowledge_warning only; anything
// else falls through to a generic events-channel line).
constexpr std::array<const char *, 2> USER_MOD_ACTION_ALLOWED = {
    "warn",
    "acknowledge_warning",
};

bool isAllowedUserModAction(QStringView action)
{
    for (const char *allowed : USER_MOD_ACTION_ALLOWED)
    {
        if (action == QLatin1String(allowed))
        {
            return true;
        }
    }
    return false;
}

bool handleUserModerationAction(const QJsonObject &data,
                                PubSubEvent &event)
{
    const QString action = data[QStringLiteral("action")].toString();
    const QString channelId = data[QStringLiteral("channel_id")].toString();
    const QString reason = data[QStringLiteral("reason")].toString();
    if (action.isEmpty() || channelId.isEmpty() ||
        !isAllowedUserModAction(action))
    {
        return false;
    }

    QString text = QStringLiteral("%1 in %2").arg(
        action, describeChannel(channelId));
    if (!reason.isEmpty())
    {
        text += QStringLiteral(", reason: %1").arg(reason);
    }
    event.displayText = text;

    // events.js L27: only act when the action targets our own user id
    // (the topic is user-scoped already, but transcribe the filter anyway).
    const QString targetId = data[QStringLiteral("target_id")].toString();
    if (!targetId.isEmpty() && targetId != event.channelId)
    {
        return false;
    }

    if (action == QLatin1String("warn"))
    {
        // Surface in the channel (decision P2-Q2), then auto-acknowledge if
        // the user enabled it (decision P2-Q1; reference events.js L60).
        auto channel = getApp()->getTwitch()->getChannelOrEmptyByID(channelId);
        if (!channel->isEmpty())
        {
            channel->addSystemMessage(text);
        }
        if (!warnRecentlyIn(channelId))
        {
            acknowledgeWarningFor(channelId);
        }
    }
    return true;
}

// community-points-user-v1 (client.js doc comment L36-46):
//   {"type":"points-spent",
//    "data":{"timestamp":,"balance":{"user_id","channel_id","balance"}}}
bool handlePointsSpent(const QJsonObject &payload, PubSubEvent &event)
{
    const QString type = payload[QStringLiteral("type")].toString();
    if (type != QLatin1String("points-spent"))
    {
        return false;  // anything else: generic events-channel line
    }

    const QJsonObject data = payload[QStringLiteral("data")].toObject();
    const QJsonObject balance = data[QStringLiteral("balance")].toObject();
    const int newBalance = balance[QStringLiteral("balance")].toInt();
    event.displayText = QStringLiteral("channel points balance is now %1")
                            .arg(newBalance);
    return true;
}

}  // namespace

void ensureHermesUserTopics()
{
    QString err;
    auto token = LimerinoAuth::resolveCurrentUserToken(&err);
    if (!token.hasToken())
    {
        return;  // the controller's never-submitted AuthBlocked path covers it
    }

    auto *c = getPubSubController();
    const QString &uid = token.userId;
    c->ensureTopic(QStringLiteral("chatrooms-user-v1.%1").arg(uid),
                   PubSubTopicAuth::User);
    c->ensureTopic(QStringLiteral("community-points-user-v1.%1").arg(uid),
                   PubSubTopicAuth::User);
    c->ensureTopic(QStringLiteral("predictions-user-v1.%1").arg(uid),
                   PubSubTopicAuth::User);
    // P3: decided to include the follows topic (reference USER_SUBS, L110).
    c->ensureTopic(QStringLiteral("follows.%1").arg(uid), PubSubTopicAuth::User);
}

void installHermesUserTopicHandlers(LimerinoPubSubController &controller)
{
    controller.registerTopicHandler(
        QStringLiteral("chatrooms-user-v1."),
        [](const QString & /*topic*/, const QJsonObject &payload,
           PubSubEvent &event) {
            const QString type =
                payload[QStringLiteral("type")].toString();
            if (type == QLatin1String("user_moderation_action"))
            {
                return handleUserModerationAction(
                    payload[QStringLiteral("data")].toObject(), event);
            }
            return false;
        });

    controller.registerTopicHandler(
        QStringLiteral("community-points-user-v1."),
        [](const QString & /*topic*/, const QJsonObject &payload,
           PubSubEvent &event) {
            return handlePointsSpent(payload, event);
        });

    // user_moderation_action (events.js L21); referenced action trust comes
    // from the subscription (chatrooms-user-v1.USERID) and is only acknowledged.
    controller.registerKnownEventType(QStringLiteral("user_moderation_action"));

    // predictions-user-v1: no reference payload shape. Only pre-register the
    // likely ones (Q3-you-allowed) so the filter dialog has them early.
    controller.registerKnownEventType(QStringLiteral("prediction-event"));
    controller.registerKnownEventType(QStringLiteral("prediction-prediction"));

    controller.registerTopicHandler(
        QStringLiteral("predictions-user-v1."),
        [](const QString & /*topic*/, const QJsonObject & /*payload*/,
           PubSubEvent & /*event*/) {
            return false;
        });

    // P3-addendum (user-selected): follows (client.js USER_SUBS L110 active).
    // events.js doesn't have a handler; reference shape appears in the
    // client.js docblock: type "user-followed" / "user-unfollowed",
    // payload {timestamp, target_display_name, target_username, target_user_id}.
    controller.registerKnownEventType(QStringLiteral("user-followed"));
    controller.registerKnownEventType(QStringLiteral("user-unfollowed"));
    controller.registerTopicHandler(
        QStringLiteral("follows."),
        [](const QString & /*topic*/, const QJsonObject &payload,
           PubSubEvent &event) {
            const QString type = payload[QStringLiteral("type")].toString();
            if (type == QLatin1String("user-followed"))
            {
                const QString name =
                    payload[QStringLiteral("target_display_name")].toString();
                event.displayText =
                    QStringLiteral("%1 followed").arg(
                        name.isEmpty()
                            ? payload[QStringLiteral("target_username")]
                                  .toString()
                            : name);
                return true;
            }
            if (type == QLatin1String("user-unfollowed"))
            {
                // unfollowed shape omits the display name (client.js L49-69):
                // use the raw user id.
                event.displayText = QStringLiteral("user unfollowed (%1)")
                                        .arg(payload[QStringLiteral(
                                                          "target_user_id")]
                                                 .toString());
                return true;
            }
            return false;
        });
}

}  // namespace chatterino::limerino
