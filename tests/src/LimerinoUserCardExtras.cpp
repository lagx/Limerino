// SPDX-License-Identifier: MIT
// Response-parse tests for the usercard "extras" combined GQL document
// (language tag / primary team / subscription detail). Exercises
// LimerinoAuth::gql::parseUserCardExtras() over crafted data objects; no
// network, no widget, no Application instance required.

#include "providers/limerino/gql/LimerinoUserCardExtras.hpp"

#include <gtest/gtest.h>
#include <QJsonObject>

using namespace chatterino::LimerinoAuth::gql;

namespace {

// Build the `data.user` object for one fetch. Empty/default strings and
// empty objects are the "absent" state. To leave a field out entirely, pass
// std::nullopt-style via the bools below.
QJsonObject makeUser(const QString &languageTag = QString(),
                     bool includeSettings = true,
                     const QString &teamName = QString(),
                     bool includePrimaryTeam = true,
                     bool includeSubscription = true,
                     const QString &platform = QString(),
                     const QString &tier = QString(),
                     bool purchasedWithPrime = false, bool isGift = false,
                     int tenureMonths = 0)
{
    QJsonObject user;

    if (includeSettings)
    {
        QJsonObject settings;
        if (!languageTag.isEmpty())
        {
            settings.insert(QStringLiteral("preferredLanguageTag"),
                            languageTag);
        }
        user.insert(QStringLiteral("settings"), settings);
    }

    if (includePrimaryTeam)
    {
        QJsonObject team;
        if (!teamName.isEmpty())
        {
            team.insert(QStringLiteral("name"), teamName);
        }
        user.insert(QStringLiteral("primaryTeam"), team);
    }

    if (includeSubscription)
    {
        QJsonObject rel;

        QJsonObject sub;
        if (!platform.isEmpty())
        {
            sub.insert(QStringLiteral("platform"), platform);
        }
        if (!tier.isEmpty())
        {
            sub.insert(QStringLiteral("tier"), tier);
        }
        if (purchasedWithPrime)
        {
            sub.insert(QStringLiteral("purchasedWithPrime"), true);
        }
        if (isGift)
        {
            sub.insert(QStringLiteral("gift"),
                       QJsonObject{{QStringLiteral("isGift"), true}});
        }
        rel.insert(QStringLiteral("subscriptionBenefit"), sub);

        if (tenureMonths > 0)
        {
            rel.insert(QStringLiteral("subscriptionTenure"),
                       QJsonObject{{QStringLiteral("months"), tenureMonths}});
        }

        user.insert(QStringLiteral("relationship"), rel);
    }

    return user;
}

}  // namespace

TEST(LimerinoUserCardExtrasParse, FullResponse)
{
    const auto user = makeUser(
        /*languageTag*/ QStringLiteral("en"),
        /*includeSettings*/ true,
        /*teamName*/ QStringLiteral("shadiversity"),
        /*includePrimaryTeam*/ true,
        /*includeSubscription*/ true, QStringLiteral("android"),
        QStringLiteral("3000"), /*prime*/ false, /*gift*/ false,
        /*tenure*/ 14);

    const auto out = parseUserCardExtras(user, false, false);

    EXPECT_EQ(out.preferredLanguageTag, QStringLiteral("en"));
    EXPECT_EQ(out.primaryTeamName, QStringLiteral("shadiversity"));
    ASSERT_TRUE(out.subscription.has_value());
    EXPECT_EQ(out.subscription->platform, QStringLiteral("android"));
    EXPECT_EQ(out.subscription->tier, QStringLiteral("3000"));
    EXPECT_EQ(out.subscription->tenureMonths, 14);
    EXPECT_FALSE(out.subscription->purchasedWithPrime);
    EXPECT_FALSE(out.subscription->isGift);
}

TEST(LimerinoUserCardExtrasParse, EmptyUserObject)
{
    // A `user` node with no extras branches at all (or entirely absent fields).
    const auto out = parseUserCardExtras(QJsonObject{}, false, false);

    EXPECT_TRUE(out.preferredLanguageTag.isEmpty());
    EXPECT_TRUE(out.primaryTeamName.isEmpty());
    EXPECT_FALSE(out.subscription.has_value());
    EXPECT_TRUE(out.empty());
}

TEST(LimerinoUserCardExtrasParse, PartialAfterErrorsArray)
{
    // The transport level treats a populated GQL errors[] as a whole-request
    // failure (rule 9), so this test instead pins down the per-field null
    // behaviour that partial success leaves in the data object:
    //   * settings present, team explicitly null, relationship null
    //   * result: language survives, team/subscription both absent,
    //     one bad field never blanks a sibling.
    QJsonObject user;
    QJsonObject settings;
    settings.insert(QStringLiteral("preferredLanguageTag"),
                    QStringLiteral("fr"));
    user.insert(QStringLiteral("settings"), settings);
    user.insert(QStringLiteral("primaryTeam"), QJsonValue::Null);
    user.insert(QStringLiteral("relationship"), QJsonValue::Null);

    const auto out = parseUserCardExtras(user, false, false);

    EXPECT_EQ(out.preferredLanguageTag, QStringLiteral("fr"));
    EXPECT_TRUE(out.primaryTeamName.isEmpty());
    EXPECT_FALSE(out.subscription.has_value());
}

TEST(LimerinoUserCardExtrasParse, NullTeam)
{
    // primaryTeam silently null for a user on no team is the common case.
    const auto user = makeUser(QStringLiteral("de"), true, QString(), true,
                               false);

    const auto out = parseUserCardExtras(user, false, false);

    EXPECT_EQ(out.preferredLanguageTag, QStringLiteral("de"));
    EXPECT_TRUE(out.primaryTeamName.isEmpty());
    EXPECT_FALSE(out.subscription.has_value());
}

TEST(LimerinoUserCardExtrasParse, NullSubscription)
{
    // relationship present but subscriptionBenefit null (target not subbed).
    QJsonObject user;
    user.insert(QStringLiteral("relationship"),
                QJsonObject{{QStringLiteral("subscriptionBenefit"),
                             QJsonValue::Null}});

    const auto out = parseUserCardExtras(user, false, false);

    EXPECT_FALSE(out.subscription.has_value());
    // Degraded field-by-field: sibling fields still parse.
    EXPECT_TRUE(out.preferredLanguageTag.isEmpty());
    EXPECT_TRUE(out.primaryTeamName.isEmpty());
}
