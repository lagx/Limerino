// SPDX-License-Identifier: MIT

#include "providers/limerino/pubsub/HermesChannelTopics.hpp"

#include "providers/limerino/pubsub/LimerinoPubSubController.hpp"
#include "providers/limerino/pubsub/LimerinoPubSubTopics.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

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
// REFERENCE NOTE: polls and predictions-channel-v1 are present in
// client.js's *commented-out* CHANNEL_SUBS lists but have no events.js
// handler. Event-type strings here are therefore documented guesses --
// these become visible the moment a real one arrives in the stream. Any
// payload shape mismatch falls through to the generic PubSubEvent display in
// the events channel instead of crashing.

QString describePrediction(const QJsonObject &payload)
{
    const QString type = payload[QStringLiteral("type")].toString();
    const QJsonObject data = payload[QStringLiteral("data")].toObject();
    const QJsonObject prediction = data[QStringLiteral("prediction")].toObject();

    // "prediction-created", "prediction-updated", "prediction-ended" are
    // guesses, no reference (see REFERENCE NOTE above). Fall through to the
    // type string in the events channel for verification.
    const QString title = prediction[QStringLiteral("title")].toString();
    const QJsonArray options = prediction[QStringLiteral("options")].toArray();

    QString opts;
    for (const QJsonValue &v : options)
    {
        if (!opts.isEmpty())
        {
            opts += QStringLiteral(" / ");
        }
        opts += v.toObject()[QStringLiteral("title")].toString();
    }

    if (!title.isEmpty() && !opts.isEmpty())
    {
        return QStringLiteral("%1 [%2]").arg(title, opts);
    }
    return title.isEmpty() ? type : title;
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
    event.displayText = describePrediction(payload);
    return true;
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
