// SPDX-License-Identifier: MIT
// State-machine tests for the Hermes live-updates controller. No network: a
// recording IPubSubSink double drives the transport signals.

#include "providers/limerino/pubsub/LimerinoPubSubController.hpp"

#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/pubsub/HermesChannelTopics.hpp"
#include "Test.hpp"

#include <QJsonObject>
#include <QString>
#include <QtCore/qtestsupport_core.h>

#include <vector>

using namespace chatterino;
using namespace chatterino::limerino;
using namespace std::chrono_literals;

namespace {

class FakeSink : public IPubSubSink
{
public:
    struct Call {
        QString how;  // "listen" | "unlisten" | "relisten" | "retryAuth"
        QString topic;
        QString key;
    };

    std::vector<Call> calls;
    std::unordered_map<QString, QString> subs;

    size_t countTopic(const QString &how, const QString &topic) const
    {
        size_t n = 0;
        for (const auto &c : this->calls)
        {
            if (c.how == how && c.topic == topic)
            {
                n++;
            }
        }
        return n;
    }

    void listen(const QString &topic, const QString &tokenKey) override
    {
        this->calls.push_back({"listen", topic, tokenKey});
        this->subs[topic] = tokenKey;
    }
    void unlisten(const QString &topic) override
    {
        this->calls.push_back({"unlisten", topic, {}});
        this->subs.erase(topic);
    }
    void relisten(const QString &topic, const QString &tokenKey) override
    {
        this->calls.push_back({"relisten", topic, tokenKey});
        this->subs[topic] = tokenKey;
    }

    std::unordered_map<QString, QString> subscribedTopics() const override
    {
        return this->subs;
    }

    PubSubTransportStatus transportStatus() const override
    {
        return {};
    }

    void retryAuthentication(const QString &tokenKey) override
    {
        this->calls.push_back({"retryAuth", {}, tokenKey});
    }

    pajlada::Signals::Signal<const QString &> sigSubscribeSucceeded;
    pajlada::Signals::Signal<const QString &, const QString &>
        sigSubscribeFailed;
    pajlada::Signals::Signal<const QString &, const QString &> sigAuthSucceeded;
    pajlada::Signals::Signal<const QString &, const QString &> sigAuthFailed;
    pajlada::Signals::Signal<const QString &, const QString &>
        sigAuthUnavailable;
    pajlada::Signals::Signal<const QString &, const QJsonObject &>
        sigTopicMessage;

    pajlada::Signals::Signal<const QString &> &subscribeSucceeded() override
    {
        return this->sigSubscribeSucceeded;
    }
    pajlada::Signals::Signal<const QString &, const QString &>
        &subscribeFailed() override
    {
        return this->sigSubscribeFailed;
    }
    pajlada::Signals::Signal<const QString &, const QString &>
        &authSucceeded() override
    {
        return this->sigAuthSucceeded;
    }
    pajlada::Signals::Signal<const QString &, const QString &>
        &authFailed() override
    {
        return this->sigAuthFailed;
    }
    pajlada::Signals::Signal<const QString &, const QString &>
        &authUnavailable() override
    {
        return this->sigAuthUnavailable;
    }
    pajlada::Signals::Signal<const QString &, const QJsonObject &>
        &topicMessage() override
    {
        return this->sigTopicMessage;
    }
};

constexpr LimerinoPubSubController::Config TEST_CONFIG{
    std::chrono::milliseconds(20), 3, 2};

const LimerinoPubSubController::TopicStatus *statusOf(
    const LimerinoPubSubController *controller, const QString &topic)
{
    // Lookup helper: statuses don't keep insertion order.
    static std::vector<LimerinoPubSubController::TopicStatus> scratch;
    scratch = controller->topicStatuses();
    for (const auto &s : scratch)
    {
        if (s.topic == topic)
        {
            return &s;
        }
    }
    return nullptr;
}

}  // namespace

TEST(LimerinoPubSubController, UnauthTopicActivates)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    controller.ensureTopic("raid.1234", PubSubTopicAuth::None);
    ASSERT_EQ(sinkPtr->countTopic("listen", "raid.1234"), 1);
    ASSERT_TRUE(sinkPtr->subs.at("raid.1234") == QStringLiteral(""));

    sinkPtr->sigSubscribeSucceeded.invoke("raid.1234");
    const auto *status = statusOf(&controller, "raid.1234");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Active);
    ASSERT_EQ(status->attempts, 0);
}

TEST(LimerinoPubSubController, FailedSubscribeRetriesThenSucceeds)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    controller.ensureTopic("raid.1234", PubSubTopicAuth::None);

    sinkPtr->sigSubscribeFailed.invoke("raid.1234", "boom");
    const auto *status = statusOf(&controller, "raid.1234");
    ASSERT_TRUE(status->state ==
                LimerinoPubSubController::TopicState::Retrying);
    ASSERT_EQ(status->attempts, 1);

    QTest::qWait(200);
    ASSERT_EQ(sinkPtr->countTopic("relisten", "raid.1234"), 1);

    sinkPtr->sigSubscribeSucceeded.invoke("raid.1234");
    status = statusOf(&controller, "raid.1234");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Active);
    ASSERT_EQ(status->attempts, 0);
}

TEST(LimerinoPubSubController, FailedSubscribeTerminatesAndManualRetry)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    controller.ensureTopic("raid.1234", PubSubTopicAuth::None);

    for (int i = 0; i < 3; ++i)  // TEST_CONFIG.maxAttempts = 3
    {
        sinkPtr->sigSubscribeFailed.invoke("raid.1234", "err");
        QTest::qWait(100);  // let any retry fire
    }
    const auto *status = statusOf(&controller, "raid.1234");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Failed);
    ASSERT_EQ(status->attempts, 3);
    ASSERT_TRUE(status->lastError == QStringLiteral("err"));

    // Terminal failure produces a visible diagnostics delta.
    auto snapshot = controller.diagSnapshot();
    ASSERT_EQ(snapshot.topicsFailed, 1);
    ASSERT_FALSE(snapshot.lastError.isEmpty());

    controller.retryFailed();
    status = statusOf(&controller, "raid.1234");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Pending);
    ASSERT_EQ(status->attempts, 0);
    ASSERT_GE(sinkPtr->countTopic("listen", "raid.1234"), 2);
}

TEST(LimerinoPubSubController, UserTopicBlockedWithoutAuthThenUnblocked)
{
    bool resolvable = false;
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink),
        [&resolvable](PubSubTopicAuth) -> PubSubTokenResolution {
            if (resolvable)
            {
                return {"tok", "u42", {}};
            }
            return {{}, {}, "no account"};
        },
        TEST_CONFIG);

    controller.ensureTopic("chatrooms-user-v1.u42", PubSubTopicAuth::User);
    ASSERT_EQ(sinkPtr->calls.size(), 0);  // never submitted (rule 5)
    const auto *status = statusOf(&controller, "chatrooms-user-v1.u42");
    ASSERT_TRUE(status->state ==
                LimerinoPubSubController::TopicState::AuthBlocked);
    ASSERT_TRUE(status->lastError == QStringLiteral("no account"));

    // Account appears: reconcile un-blocks and subscribes with the new key.
    resolvable = true;
    LimerinoAuth::accountsChanged.invoke();
    ASSERT_EQ(sinkPtr->countTopic("listen", "chatrooms-user-v1.u42"), 1);
    ASSERT_TRUE(sinkPtr->subs.at("chatrooms-user-v1.u42") ==
                QStringLiteral("u42"));
    status = statusOf(&controller, "chatrooms-user-v1.u42");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Pending);
}

TEST(LimerinoPubSubController, AccountSwitchRekeysTopic)
{
    QString currentUserId = "u1";
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink),
        [&currentUserId](PubSubTopicAuth) -> PubSubTokenResolution {
            if (currentUserId.isEmpty())
            {
                return {{}, {}, "no account"};
            }
            return {"tok", currentUserId, {}};
        },
        TEST_CONFIG);

    controller.ensureTopic("chatrooms-user-v1.u1", PubSubTopicAuth::User);
    ASSERT_TRUE(sinkPtr->subs.at("chatrooms-user-v1.u1") ==
                QStringLiteral("u1"));

    // Switch: the topic is re-issued as the new user's own topic string.
    currentUserId = "u2";
    LimerinoAuth::accountsChanged.invoke();
    ASSERT_EQ(sinkPtr->countTopic("unlisten", "chatrooms-user-v1.u1"), 1);
    ASSERT_EQ(sinkPtr->subs.count("chatrooms-user-v1.u1"), 0);
    ASSERT_TRUE(sinkPtr->subs.at("chatrooms-user-v1.u2") ==
                QStringLiteral("u2"));

    // Account disappears: the topic must be unsubscribed entirely (rule 5).
    currentUserId.clear();
    LimerinoAuth::accountsChanged.invoke();
    ASSERT_EQ(sinkPtr->countTopic("unlisten", "chatrooms-user-v1.u2"), 1);
    ASSERT_EQ(sinkPtr->subs.count("chatrooms-user-v1.u2"), 0);
}

TEST(LimerinoPubSubController, SweepRemovesForeignUserTopics)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {"tok", "u1", {}};
        },
        TEST_CONFIG);

    controller.ensureTopic("chatrooms-user-v1.u1", PubSubTopicAuth::User);
    controller.ensureTopic("raid.1234", PubSubTopicAuth::None);

    // A stale subscription from another account lingers on the connection.
    sinkPtr->subs["chatrooms-user-v1.u999"] = "u999";

    LimerinoAuth::accountsChanged.invoke();

    ASSERT_EQ(sinkPtr->countTopic("unlisten", "chatrooms-user-v1.u999"), 1);
    ASSERT_EQ(sinkPtr->subs.count("chatrooms-user-v1.u999"), 0);
    // Our own topics stay untouched.
    ASSERT_EQ(sinkPtr->subs.count("chatrooms-user-v1.u1"), 1);
    ASSERT_EQ(sinkPtr->subs.count("raid.1234"), 1);
}

TEST(LimerinoPubSubController, AuthFailuresFailTopicFast)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {"tok", "u1", {}};
        },
        TEST_CONFIG);

    controller.ensureTopic("chatrooms-user-v1.u1", PubSubTopicAuth::User);

    sinkPtr->sigAuthFailed.invoke("u1", "ERR");
    const auto *status = statusOf(&controller, "chatrooms-user-v1.u1");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Pending);

    sinkPtr->sigAuthFailed.invoke("u1", "ERR");  // TEST_CONFIG.maxAuthFailures = 2
    status = statusOf(&controller, "chatrooms-user-v1.u1");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Failed);
    ASSERT_TRUE(status->lastError == QStringLiteral("ERR"));

    // The auth gate now refuses live token resolves for this key.
    QString reason;
    ASSERT_FALSE(controller.resolveLiveToken("u1", &reason).has_value());
    ASSERT_FALSE(reason.isEmpty());

    controller.retryFailed();
    status = statusOf(&controller, "chatrooms-user-v1.u1");
    ASSERT_TRUE(status->state == LimerinoPubSubController::TopicState::Pending);
}

TEST(LimerinoPubSubController, NotificationBecomesGenericEvent)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    controller.ensureTopic("raid.1234", PubSubTopicAuth::None);

    std::vector<PubSubEvent> events;
    controller.eventProduced.connect(
        [&events](const PubSubEvent &event) { events.push_back(event); });

    QJsonObject payload{{"type", "raid_update_v2"},
                        {"raid", QJsonObject{{"id", "r1"}}}};
    sinkPtr->sigTopicMessage.invoke("raid.1234", payload);

    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(events[0].topic == QStringLiteral("raid.1234"));
    ASSERT_TRUE(events[0].channelId == QStringLiteral("1234"));
    ASSERT_TRUE(events[0].eventType == QStringLiteral("raid_update_v2"));
    ASSERT_FALSE(events[0].displayText.isEmpty());
    ASSERT_TRUE(
        controller.knownEventTypes().contains(QStringLiteral("raid_update_v2")));
}

TEST(LimerinoPubSubController, RegisteredHandlerFormatsEvent)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    controller.registerTopicHandler(
        "raid.",
        [](const QString &, const QJsonObject &payload, PubSubEvent &event) {
            event.displayText = QStringLiteral("raid event %1")
                                    .arg(payload["type"].toString());
            return true;
        });

    std::vector<PubSubEvent> events;
    controller.eventProduced.connect(
        [&events](const PubSubEvent &event) { events.push_back(event); });

    sinkPtr->sigTopicMessage.invoke("raid.1234",
                                    QJsonObject{{"type", "raid_update_v2"}});

    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(events[0].displayText ==
                QStringLiteral("raid event raid_update_v2"));
}

// ---- P1: channel-topic handlers ----

TEST(LimerinoPubSubP1, HandlersInstallAndRaidFormats)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    limerino::installHermesChannelTopicHandlers();

    std::vector<PubSubEvent> events;
    controller.eventProduced.connect(
        [&events](const PubSubEvent &event) { events.push_back(event); });

    const QJsonObject raid{
        {"id", "raid-1"},
        {"source_id", "9001"},
        {"target_login", "target"},
        {"target_display_name", "Target"},
    };

    // Reference shape: raid at top level (events.js L79).
    sinkPtr->sigTopicMessage.invoke(
        "raid.2500",
        QJsonObject{{"type", "raid_update_v2"}, {"raid", raid}});

    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(events[0].displayText.contains(QStringLiteral("Target")));
    ASSERT_TRUE(events[0].channelId == QStringLiteral("2500"));

    // Fallback shape: raid nested under data.
    sinkPtr->sigTopicMessage.invoke(
        "raid.2500",
        QJsonObject{{"type", "raid_update_v2"},
                    {"data", QJsonObject{{"raid", raid}}}});
    ASSERT_EQ(events.size(), 2);
    ASSERT_TRUE(events[1].displayText.contains(QStringLiteral("Target")));
}

TEST(LimerinoPubSubP1, UnknownTypesFallBackToTypeString)
{
    auto sink = std::make_unique<FakeSink>();
    auto *sinkPtr = sink.get();
    LimerinoPubSubController controller(
        std::move(sink), [](PubSubTopicAuth) -> PubSubTokenResolution {
            return {};
        },
        TEST_CONFIG);

    limerino::installHermesChannelTopicHandlers();

    std::vector<PubSubEvent> events;
    controller.eventProduced.connect(
        [&events](const PubSubEvent &event) { events.push_back(event); });

    sinkPtr->sigTopicMessage.invoke(
        "polls.99", QJsonObject{{"type", "weird-poll-thing"}});

    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(events[0].displayText.contains(QStringLiteral("weird-poll-thing")));
}
