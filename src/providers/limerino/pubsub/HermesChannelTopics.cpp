// SPDX-License-Identifier: MIT

#include "providers/limerino/pubsub/HermesChannelTopics.hpp"

#include "providers/limerino/pubsub/LimerinoPubSubController.hpp"
#include "providers/limerino/pubsub/LimerinoPubSubTopics.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace chatterino::limerino {

namespace {

// --- raid.<channelID> -------------------------------------------------------
//
// Reference transcripts: events.js L78-129 (works on the raid.<channelID>
// topic; type "raid_update_v2").
//
// Fields consumed (transcribed):
//   raid.id / raid.source_id - identity + cooldown key
//   raid.target_login / raid.target_display_name - destination
//   raid.creator_id - only interesting when != source_id (summoned)
//
// The reference tolerates raid at the top level ("msg.raid") or nested
// ("msg.data.raid") - same fallback kept here.

QString formatRaidDisplay(const PubSubEvent & /*event*/,
                          const QJsonObject &raid)
{
    const QString from = raid[QStringLiteral("source_id")].toString();
    const QString toL = raid[QStringLiteral("target_login")].toString();
    const QString toD = raid[QStringLiteral("target_display_name")].toString();
    const QString to = toD.isEmpty() ? toL : toD;

    QString text = QStringLiteral("raid from %1 to %2").arg(from, to);
    const QString creatorId = raid[QStringLiteral("creator_id")].toString();
    if (!creatorId.isEmpty() && creatorId != from)
    {
        text += QStringLiteral(" (created by %1)").arg(creatorId);
    }
    return text;
}

// --- polls.<channelID> / predictions-channel-v1.<channelID> ----------------
//
// predictions-* shapes are authoritative from newpubsubhermesreference/
// hermes/events/prediction.js (no longer guesses). Channel + user topics both
// emit "event-created" / "event-updated" whose data.event carries:
//   id, channel_id, created_at, title, status, prediction_window_seconds,
//   outcomes[], winning_outcome_id, created_by.user_id
//
// polls.<channelID> has no handler in the new reference; its describePoll
// reads the shape formerly documented for it.

// One-line summary of a prediction event. `forUserTopic` selects the
// "your ..." phrasing used on the user topic. Returns empty when unparseable.
QString sharedPredictionEventText(const QJsonObject &data, const QString &type,
                                  bool forUserTopic)
{
    const QJsonObject event = data[QStringLiteral("event")].toObject();
    const QString title = event[QStringLiteral("title")].toString();
    if (title.isEmpty())
    {
        return {};
    }
    const QJsonArray outcomes = event[QStringLiteral("outcomes")].toArray();
    QString opts;
    for (const QJsonValue &v : outcomes)
    {
        if (!opts.isEmpty())
        {
            opts += QStringLiteral(" / ");
        }
        opts += v.toObject()[QStringLiteral("title")].toString();
    }
    const QString who = forUserTopic ? QStringLiteral("your ")
                                     : QString();
    if (type == QLatin1String("event-created"))
    {
        return opts.isEmpty()
                   ? QStringLiteral("%1prediction started: %2").arg(who, title)
                   : QStringLiteral("%1prediction started: %2 [%3]")
                         .arg(who, title, opts);
    }
    if (type == QLatin1String("event-updated"))
    {
        const QString status = event[QStringLiteral("status")].toString();
        QString suffix;
        if (status == QLatin1String("ACTIVE"))
        {
            suffix = QStringLiteral("active");
        }
        else if (status == QLatin1String("LOCKED"))
        {
            suffix = QStringLiteral("locked");
        }
        else if (status == QLatin1String("RESOLVED"))
        {
            suffix = QStringLiteral("resolved");
        }
        else if (status == QLatin1String("CANCEL_PENDING") ||
                 status == QLatin1String("CANCELED"))
        {
            suffix = QStringLiteral("cancelled");
        }
        else
        {
            suffix = status;
        }
        return suffix.isEmpty()
                   ? QStringLiteral("%1prediction updated: %2").arg(who, title)
                   : QStringLiteral("%1prediction updated: %2 (%3)")
                         .arg(who, title, suffix);
    }
    return {};
}

QString describePoll(const QJsonObject &payload)
{
    const QString type = payload[QStringLiteral("type")].toString();
    const QJsonObject data = payload[QStringLiteral("data")].toObject();
    const QString title = data[QStringLiteral("poll")]
                            .toObject()[QStringLiteral("title")]
                            .toString();
    return title.isEmpty() ? type : title;
}

// Event handlers. true = consumed; the generic PubSubEvent still fires so the
// events channel receives a line regardless.
bool handleRaid(const QString & /*topic*/, const QJsonObject &payload,
                PubSubEvent &event)
{
    const QString type = payload[QStringLiteral("type")].toString();
    // Reference callback name events.js L78: raid_update_v2
    if (type != QLatin1String("raid_update_v2"))
    {
        return false;
    }

    // Payload fallback: events.js L79 reads msg.raid; tolerate data.raid too
    // (seen via the notification-wrapping pubsub path in client.js).
    QJsonObject raid = payload[QStringLiteral("raid")].toObject();
    if (raid.isEmpty())
    {
        raid = payload[QStringLiteral("data")].toObject()[QStringLiteral("raid")]
                   .toObject();
    }
    if (raid.isEmpty())
    {
        return false;
    }

    event.displayText = formatRaidDisplay(event, raid);
    return true;
}

bool handlePredictionsChannel(const QString & /*topic*/,
                              const QJsonObject &payload, PubSubEvent &event)
{
    const QString type = payload[QStringLiteral("type")].toString();
    if (type != QLatin1String("event-created") &&
        type != QLatin1String("event-updated"))
    {
        return false;  // unknown type string: generic events-channel line
    }
    event.displayText = sharedPredictionEventText(
        payload[QStringLiteral("data")].toObject(), type, false);
    return !event.displayText.isEmpty();
}

bool handlePollChannel(const QString & /*topic*/, const QJsonObject &payload,
                       PubSubEvent &event)
{
    event.displayText = describePoll(payload);
    return true;
}

}  // namespace

void installHermesChannelTopicHandlers(LimerinoPubSubController &controller)
{
    controller.registerTopicHandler(QStringLiteral("raid."), &handleRaid);
    controller.registerTopicHandler(QStringLiteral("predictions-channel-v1."),
                                    &handlePredictionsChannel);
    controller.registerTopicHandler(QStringLiteral("polls."),
                                    &handlePollChannel);

    // Reference event type strings (pre-register so the filter dialog has
    // known entries before the first stream event).
    controller.registerKnownEventType(QStringLiteral("raid_update_v2"));
    controller.registerKnownEventType(QStringLiteral("event-created"));
    controller.registerKnownEventType(QStringLiteral("event-updated"));
}

void ensureHermesChannelTopics(const TwitchChannel &channel)
{
    const QString id = channel.roomId();
    if (id.isEmpty())
    {
        return;
    }
    auto *c = getPubSubController();
    c->ensureTopic(QStringLiteral("raid.%1").arg(id), PubSubTopicAuth::None);
    c->ensureTopic(QStringLiteral("polls.%1").arg(id), PubSubTopicAuth::None);
    c->ensureTopic(QStringLiteral("predictions-channel-v1.%1").arg(id),
                   PubSubTopicAuth::None);
}

}  // namespace chatterino::limerino
