// SPDX-License-Identifier: MIT

#include "limerino/PubSubEventsChannel.hpp"

#include "Application.hpp"
#include "providers/limerino/pubsub/LimerinoPubSubController.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <pajlada/signals/signalholder.hpp>

namespace chatterino::limerino {

namespace {

pajlada::Signals::SignalHolder &channelConnections()
{
    static auto *holder = new pajlada::Signals::SignalHolder();
    return *holder;
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
                created->addSystemMessage(event.displayText);
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
