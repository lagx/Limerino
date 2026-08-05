// SPDX-License-Identifier: MIT

#include "limerino/PubSubEventsChannel.hpp"

#include "Application.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/limerino/pubsub/LimerinoPubSubController.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <pajlada/signals/signalholder.hpp>

#include <QJsonDocument>
#include <QTime>

namespace chatterino::limerino {

namespace {

pajlada::Signals::SignalHolder &channelConnections()
{
    static auto *holder = new pajlada::Signals::SignalHolder();
    return *holder;
}

// Category chip: short label + theme-derived color (never hardcoded).
// Buckets: accent = attention (moderation, raid), link = interactive/info
// (points, prediction, poll), system = neutral (follow, unknown).
struct EventStyle {
    QString label;
    MessageColor color;
};

EventStyle styleForCategory(const QString &category)
{
    if (category == u"moderation")
    {
        return {QStringLiteral("MOD"), MessageColor(getTheme()->accent)};
    }
    if (category == u"points")
    {
        return {QStringLiteral("POINTS"),
                MessageColor(getTheme()->messages.textColors.link)};
    }
    if (category == u"prediction")
    {
        return {QStringLiteral("PREDICTION"),
                MessageColor(getTheme()->messages.textColors.link)};
    }
    if (category == u"poll")
    {
        return {QStringLiteral("POLL"),
                MessageColor(getTheme()->messages.textColors.link)};
    }
    if (category == u"raid")
    {
        return {QStringLiteral("RAID"), MessageColor(getTheme()->accent)};
    }
    if (category == u"follow")
    {
        return {QStringLiteral("FOLLOW"), MessageColor::System};
    }
    return {QStringLiteral("EVENT"), MessageColor::System};
}

// One event as a proper message: timestamp, coloured category chip, plain
// text. The chip's tooltip carries the raw topic + payload for debugging.
MessagePtr buildEventMessage(const PubSubEvent &event)
{
    const EventStyle style = styleForCategory(event.category);

    MessageBuilder builder;
    builder.emplace<TimestampElement>(QTime::currentTime());
    builder.message().flags.set(MessageFlag::System);
    builder.message().flags.set(MessageFlag::DoNotTriggerNotification);

    const QString rawJson = QString::fromUtf8(
        QJsonDocument(event.payload).toJson(QJsonDocument::Compact));
    builder.emplace<TextElement>(style.label, MessageElementFlag::Text,
                                 style.color, FontStyle::ChatMediumBold)
        ->setTooltip(event.topic + QLatin1Char('\n') + rawJson);
    builder.emplace<TextElement>(QStringLiteral(" "), MessageElementFlag::Text,
                                 MessageColor::System);
    builder.emplace<TextElement>(event.displayText, MessageElementFlag::Text,
                                 MessageColor::Text);

    // Cheap channel focus link: shown only when the topic's channel resolves
    // to an open channel whose name isn't already in the text.
    if (!event.channelId.isEmpty())
    {
        const auto channelPtr =
            getApp()->getTwitch()->getChannelOrEmptyByID(event.channelId);
        if (!channelPtr->isEmpty() &&
            !event.displayText.contains(channelPtr->getName(),
                                        Qt::CaseInsensitive))
        {
            builder.emplace<TextElement>(QStringLiteral(" "),
                                         MessageElementFlag::Text,
                                         MessageColor::System);
            builder.appendChannelName(channelPtr.get());
        }
    }

    builder.message().messageText = event.displayText;
    builder.message().searchText = event.displayText;
    return builder.release();
}

}  // namespace

const QString &pubSubEventsChannelName()
{
    static const QString name = QStringLiteral("/events");
    return name;
}

ChannelPtr pubSubEventsChannel()
{
    static ChannelPtr channel = [] {
        auto created = std::make_shared<Channel>(pubSubEventsChannelName(),
                                                 Channel::Type::Misc);
        created->addSystemMessage(QStringLiteral(
            "All live events appear here. Use the three-dots menu of this "
            "tab and choose \"Filter events...\" to pick which event types "
            "are shown."));
        channelConnections().managedConnect(
            getPubSubController()->eventProduced,
            [created](const PubSubEvent &event) {
                if (!event.eventType.isEmpty() &&
                    pubSubEventTypeHidden(event.eventType))
                {
                    return;
                }
                created->addMessage(buildEventMessage(event),
                                    MessageContext::Original);
            });
        return created;
    }();
    return channel;
}

void openPubSubEventsChannelTab()
{
    // Same new-tab pattern as the other Limerino "open chat" actions.
    auto &notebook = getApp()->getWindows()->getMainWindow().getNotebook();
    auto *container = notebook.addPage(true);
    auto *split = new Split(container);
    split->setChannel(pubSubEventsChannel());
    container->insertSplit(split);
}

bool pubSubEventTypeHidden(const QString &type)
{
    return hiddenPubSubEventTypes().contains(type, Qt::CaseInsensitive);
}

void setPubSubEventTypeHidden(const QString &type, bool hidden)
{
    QStringList hiddenList = hiddenPubSubEventTypes();
    const bool contains = hiddenList.contains(type, Qt::CaseInsensitive);
    if (hidden && !contains)
    {
        hiddenList.append(type);
    }
    else if (!hidden && contains)
    {
        hiddenList.removeIf([&](const QString &item) {
            return item.compare(type, Qt::CaseInsensitive) == 0;
        });
    }
    else
    {
        return;  // no change
    }
    getSettings()->limerinoPubSubHiddenEventTypes.setValue(hiddenList);
}

QStringList hiddenPubSubEventTypes()
{
    return getSettings()->limerinoPubSubHiddenEventTypes.getValue();
}

}  // namespace chatterino::limerino
