// SPDX-License-Identifier: MIT

#include "providers/limerino/commands/Identity.hpp"

#include "common/Channel.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/limerino/gql/LimerinoGql.hpp"
#include "providers/limerino/gql/PersistedQueries.hpp"
#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/LimerinoErrors.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "widgets/dialogs/limerino/LimerinoResultDialog.hpp"
#include "widgets/dialogs/limerino/LimerinoResultList.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace chatterino::LimerinoCommands {

namespace gql = chatterino::LimerinoAuth::gql;

namespace {

void say(const ChannelPtr &channel, const QString &text)
{
    if (channel)
    {
        channel->addSystemMessage(text);
    }
}

// Plugin's getUserId: persisted GetUserID, {login, lookupType:"ALL"}.
void fetchUserId(const QString &login,
                 const std::function<void(const QString &id)> &onFound,
                 const std::function<void(const QString &err)> &onError)
{
    QString err;
    auto token = LimerinoAuth::resolveReadToken(&err);
    if (!token.hasToken())
    {
        onError(err.isEmpty() ? LimerinoAuth::errors::tokenRequiredMessage(
                                    QStringLiteral("look up a user ID"))
                              : err);
        return;
    }

    gql::executePersisted(
        gql::PQ_GET_USER_ID,
        QJsonObject{{QStringLiteral("login"), login},
                    {QStringLiteral("lookupType"), QStringLiteral("ALL")}},
        token.token,
        [onFound](const QJsonObject &data) {
            const QString id = data[QStringLiteral("user")]
                                   .toObject()[QStringLiteral("id")]
                                   .toString();
            if (id.isEmpty())
            {
                return;  // plugin: nil -> silence
            }
            if (onFound)
            {
                onFound(id);
            }
        },
        [onError](const gql::GqlError &e) {
            if (onError)
            {
                onError(e.message);
            }
        });
}

// Plugin's getNameHistory: GET https://logs.zonian.dev/namehistory[login:]<user>
void fetchNameHistory(const QString &user,
                      const std::function<void(const QStringList &logins)> &onOk,
                      const std::function<void(const QString &err)> &onErr)
{
    QString url;
    if (user.startsWith(QStringLiteral("id:")))
    {
        url = QStringLiteral("https://logs.zonian.dev/namehistory/%1")
                  .arg(QStringView(user).mid(3));
    }
    else
    {
        url = QStringLiteral("https://logs.zonian.dev/namehistory/login:%1")
                  .arg(user);
    }

    NetworkRequest(QUrl(url), NetworkRequestType::Get)
        .timeout(20000)
        .onSuccess([onOk, onErr](NetworkResult result) {
            const QJsonArray arr =
                QJsonDocument::fromJson(result.getData()).array();
            if (arr.isEmpty())
            {
                if (onErr)
                {
                    onErr(QStringLiteral("No name history found"));
                }
                return;
            }
            QStringList logins;
            for (const QJsonValue &v : arr)
            {
                const QString login =
                    v.toObject()[QStringLiteral("user_login")].toString();
                if (!login.isEmpty())
                {
                    logins.append(login);
                }
            }
            if (logins.isEmpty())
            {
                if (onErr)
                {
                    onErr(QStringLiteral("No name history found"));
                }
                return;
            }
            if (onOk)
            {
                onOk(logins);
            }
        })
        .onError([onErr](NetworkResult result) {
            if (onErr)
            {
                onErr(LimerinoAuth::errors::describeHttpFailure(
                    result.status().value_or(0),
                    QStringLiteral("fetch name history")));
            }
        })
        .execute();
}

}  // namespace

QString uid(const CommandContext &ctx)
{
    auto *tchan = ctx.twitchChannel;
    if (tchan == nullptr)
    {
        return QStringLiteral("/uid: only available in Twitch channels");
    }

    QString login = ctx.words.value(1);
    if (login.isEmpty())
    {
        login = tchan->getName();  // default: this channel
    }
    login.remove(u'@');

    const ChannelPtr channel = ctx.channel;
    fetchUserId(
        login,
        [weak = std::weak_ptr(channel), login](const QString &id) {
            if (auto chan = weak.lock())
            {
                chan->addSystemMessage(
                    QStringLiteral("%1's Twitch ID: %2").arg(login, id));
            }
        },
        [weak = std::weak_ptr(channel)](const QString &err) {
            if (auto chan = weak.lock())
            {
                chan->addSystemMessage(err);
            }
        });
    return {};
}

QString nameHistory(const CommandContext &ctx)
{
    auto *tchan = ctx.twitchChannel;
    if (tchan == nullptr)
    {
        return QStringLiteral("/namehistory: only available in Twitch channels");
    }

    const QString user = ctx.words.value(1);
    if (user.isEmpty())
    {
        say(ctx.channel, QStringLiteral("Usage: /namehistory <user>"));
        return {};
    }

    const ChannelPtr channel = ctx.channel;
    fetchNameHistory(
        user,
        [weak = std::weak_ptr(channel)](const QStringList &logins) {
            if (auto chan = weak.lock())
            {
                chan->addSystemMessage(QStringLiteral("Username history: %1")
                                           .arg(logins.join(QStringLiteral("\n"))));
            }
        },
        [weak = std::weak_ptr(channel)](const QString &err) {
            if (auto chan = weak.lock())
            {
                chan->addSystemMessage(err);
            }
        });
    return {};
}

void showNameHistoryDialog(const QString &login, QWidget *parent)
{
    auto *dialog = new limerino::LimerinoResultDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Name history - %1").arg(login));
    dialog->resultList()->setTitleText(
        QStringLiteral("Name history for %1").arg(login));
    dialog->resultList()->setColumns({QStringLiteral("username")});
    dialog->resultList()->setStatusText(QStringLiteral("fetching..."));
    dialog->resize(360, 400);
    dialog->show();

    // Crash fix: the callbacks must not touch anything owned by the dialog
    // after it may have died (the dialog's parent popup can close it first).
    // Guard every use with the QPointer; there is no separate lifeGuard.
    fetchNameHistory(
        login,
        [guard = QPointer<limerino::LimerinoResultDialog>(dialog)](
            const QStringList &logins) {
            if (!guard)
            {
                return;
            }
            QVector<QStringList> rows;
            rows.reserve(logins.size());
            for (const QString &l : logins)
            {
                rows.append({l});
            }
            guard->resultList()->setRows(rows);
            guard->resultList()->setStatusText(
                QStringLiteral("%1 stored names").arg(logins.size()));
        },
        [guard = QPointer<limerino::LimerinoResultDialog>(dialog)](
            const QString &err) {
            if (!guard)
            {
                return;
            }
            guard->resultList()->setStatusText(err);
        });
}

QString modList(const CommandContext &ctx)
{
    auto *tchan = ctx.twitchChannel;
    if (tchan == nullptr)
    {
        return QStringLiteral("/modlist: only available in Twitch channels");
    }

    QString err;
    auto token = LimerinoAuth::resolveCurrentUserToken(&err);
    if (!token.hasToken())
    {
        say(ctx.channel, err.isEmpty()
                             ? LimerinoAuth::errors::tokenRequiredMessage(
                                   QStringLiteral("list your moderated channels"))
                             : err);
        return {};
    }

    say(ctx.channel, QStringLiteral("Refreshing moderated channels list..."));
    const ChannelPtr channel = ctx.channel;

    // /modlist always re-fetches first (cache may be stale), then displays.
    auto *invokeGuard = new QObject();
    QPointer<QObject> guard(invokeGuard);
    LimerinoAuth::refreshAccounts(
        [guard, channel, invokeGuard,
         token](const LimerinoAuth::LimerinoAuthRefreshResult & /*result*/) {
            invokeGuard->deleteLater();
            if (auto chan = channel)
            {
                LimerinoAuth::LimerinoAuthAccount self;
                for (const auto &a : LimerinoAuth::accounts())
                {
                    if (a.userId == token.userId)
                    {
                        self = a;
                        break;
                    }
                }
                if (self.moderatedChannels.isEmpty())
                {
                    chan->addSystemMessage(QStringLiteral(
                        "Moderated channels list is unavailable. "
                        "Re-check the account under Settings > Limerino."));
                    return;
                }

                chan->addSystemMessage(QStringLiteral(
                                           "Found %1 moderated channels:")
                                           .arg(self.moderatedChannels.size()));

                QVector<QStringList> rows;
                rows.reserve(self.moderatedChannels.size());
                for (const auto &c : self.moderatedChannels)
                {
                    rows.append({c.login, c.displayName});
                }

                auto *dialog = new limerino::LimerinoResultDialog;
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->setWindowTitle(QStringLiteral(
                    "Moderated channels - %1").arg(self.displayName));
                dialog->resultList()->setTitleText(QStringLiteral(
                    "%1 moderates %2 channels")
                    .arg(self.displayName.isEmpty() ? self.login
                                                    : self.displayName)
                    .arg(rows.size()));
                dialog->resultList()->setColumns(
                    {QStringLiteral("login"), QStringLiteral("display name")});
                dialog->resultList()->setRows(rows);
                dialog->resultList()->setStatusText(
                    QStringLiteral("refreshed %1")
                        .arg(self.lastValidatedAt.toString(
                            QStringLiteral("yyyy-MM-dd hh:mm"))));
                dialog->resize(480, 500);
                dialog->show();
            }
        });
    return {};
}

QVector<CommandDoc> commandDocs()
{
    return {
        {QStringLiteral("/uid"),
         QStringLiteral("/uid [user] (default: this channel)"),
         QStringLiteral("Show a user's Twitch ID (via persisted GQL GetUserID).")},
        {QStringLiteral("/namehistory"),
         QStringLiteral("/namehistory <user or id:...>"),
         QStringLiteral("Show a user's past usernames (logs.zonian.dev). Also "
                        "available as the \"Name history\" button on usercards.")},
        {QStringLiteral("/modlist"),
         QStringLiteral("/modlist"),
         QStringLiteral("Refresh and list every channel your extra-features "
                        "account moderates (Helix moderation/channels). "
                        "Also opens the result list.")},
        {QStringLiteral("/follow"),
         QStringLiteral("/follow [user] (default: this channel)"),
         QStringLiteral("Follow a channel (persisted FollowButton_FollowUser). "
                        "Also in the split 3-dot menu as \"Follow channel\". "
                        "Replaces the stock /follow.")},
        {QStringLiteral("(menus)"),
         QStringLiteral("chat settings (own channel): View followers / View following"),
         QStringLiteral("Open the followers window (searchable, since + for, "
                        "right-click to block) and the following window (live "
                        "dots, notifications, right-click to open chat/channel).")},
        {QStringLiteral("/artist, /unartist"),
         QStringLiteral("/<un>artist <username>"),
         QStringLiteral("Grant/revoke the artist role on your own channel "
                        "(grantCommunityRole/revokeCommunityRole). Buttons also "
                        "appear on usercards in your own channel.")},
        {QStringLiteral("/leadmod"),
         QStringLiteral("/leadmod <modname>"),
         QStringLiteral("Assign the lead-moderator role (AssignChannelRole, "
                        "own channel). Also a usercard button in your channel.")},
        {QStringLiteral("(avatar menu)"),
         QStringLiteral("usercard avatar right-click"),
         QStringLiteral("View channel editors / editor-in-channels (7TV v3/v4 "
                        "queries); rows link to https://7tv.app/users/<id>.")},
        {QStringLiteral("/modlogs"),
         QStringLiteral("/modlogs [user] [days|all] (default: channel, 30 days)"),
         QStringLiteral("Moderator action window: per-moderator breakdown "
                        "(clickable names → Twitch channels, numeric sort) and "
                        "full action list, newest first.")},
        {QStringLiteral("/acknowledgewarning"),
         QStringLiteral("/acknowledgewarning"),
         QStringLiteral("Acknowledge the chat warning shown by the channel.")},
        {QStringLiteral("/resub"),
         QStringLiteral("/resub [message]"),
         QStringLiteral("Share your resub notification (silent on success, "
                        "like the plugin).")},
        {QStringLiteral("/cheer"),
         QStringLiteral("/cheer <bits> [message]"),
         QStringLiteral("Send a cheer (ChatInput_SendCheer, per-plugin fields).")},
        {QStringLiteral("/displayname"),
         QStringLiteral("/displayname <name>"),
         QStringLiteral("Update your display name (updateUser mutation).")},
        {QStringLiteral("/logsextended"),
         QStringLiteral("/logsextended [-id] [user] [channel]"),
         QStringLiteral("Fetch a user's chat log into a paste on the configured "
                        "host. -id includes message ids (request keeps the id "
                        "field); without it the field is removed from the query.")},
        {QStringLiteral("(icon)"),
         QStringLiteral("channel-points icon (mod toolbar)"),
         QStringLiteral("Opens the channel-points window: create predictions "
                        "and polls (title, window, 2-10 / 2-5 options), "
                        "draft history of last 5, make prediction with "
                        "confirm, lock, payout, cancel/refund, past predictions.")},
        {QStringLiteral("/pinmessage"),
         QStringLiteral("/pinmessage <message id>"),
         QStringLiteral("Pin a message in this channel (persisted PinChatMessage).")},
        {QStringLiteral("/sendpinnedmessage"),
         QStringLiteral("/sendpinnedmessage <text>"),
         QStringLiteral("Send a message that goes straight to the pin banner.")},
        {QStringLiteral("/unpin"),
         QStringLiteral("/unpin"),
         QStringLiteral("Unpin the channel's most-recent pin (GQL). Replaces "
                        "upstream's /unpin; upstream /pin stays.")},
        {QStringLiteral("/viewpin, /getpin"),
         QStringLiteral("/viewpin"),
         QStringLiteral("Show the pinned message in a banner at the top of "
                        "this chat (exact chat rendering, dismissible, tinted "
                        "via Settings > Highlights color).")},
    };
}

}  // namespace chatterino::LimerinoCommands
