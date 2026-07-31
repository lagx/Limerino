// SPDX-License-Identifier: MIT

#include "providers/limerino/gql/LimerinoGql.hpp"

#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "providers/limerino/gql/PersistedQueries.hpp"
#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/LimerinoErrors.hpp"
#include "providers/limerino/LimerinoRateLimiter.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include <utility>

namespace chatterino::LimerinoAuth::gql {

namespace {

const QString GQL_URL = QStringLiteral("https://gql.twitch.tv/gql/");
const QString RATE_BUCKET = QStringLiteral("gql.twitch.tv");

void deliver(const QJsonDocument &doc, const GqlSuccessCallback &onSuccess,
             const GqlErrorCallback &onError)
{
    // The plugin always batches exactly one operation per POST ([ {...} ]).
    const QJsonArray batch = doc.array();
    const QJsonObject op = batch.isEmpty() ? QJsonObject()
                                           : batch.first().toObject();
    const QJsonArray errors = op[QStringLiteral("errors")].toArray();
    if (!errors.isEmpty())
    {
        GqlError err;
        err.message = errors::fromGraphQlErrors(errors);
        for (const QJsonValue &v : errors)
        {
            err.rawErrors.append(
                v.toObject()[QStringLiteral("message")].toString());
        }
        if (onError)
        {
            onError(err);
        }
        return;
    }

    if (onSuccess)
    {
        onSuccess(op[QStringLiteral("data")].toObject());
    }
}

std::function<void(NetworkResult)> makeHttpErrorHandler(
    const GqlErrorCallback &onError, const QString &action)
{
    return [onError, action](NetworkResult result) {
        GqlError err;
        err.httpFailed = true;
        err.httpStatus = result.status().value_or(0);
        err.message = errors::describeHttpFailure(err.httpStatus, action);
        if (onError)
        {
            onError(err);
        }
    };
}

void executeBody(const QByteArray &body, const QString &gqlToken,
                 const GqlSuccessCallback &onSuccess,
                 const GqlErrorCallback &onError, const QString &action,
                 int timeoutMs)
{
    LimerinoRateLimiter::instance().execute(
        RATE_BUCKET,
        [=] {
            return NetworkRequest(QUrl(GQL_URL), NetworkRequestType::Post)
                .header("Content-Type", "application/json")
                .header("client-id", LimerinoAuth::CLIENT_ID)
                .header("authorization",
                        QStringLiteral("OAuth ") + gqlToken)
                .hideRequestBody()
                .payload(body)
                .timeout(timeoutMs);
        },
        [onSuccess, onError](NetworkResult result) {
            deliver(QJsonDocument::fromJson(result.getData()), onSuccess,
                    onError);
        },
        makeHttpErrorHandler(onError, action));
}

}  // namespace

void executePersisted(const PersistedQuery &operation,
                      const QJsonObject &variables, const QString &gqlToken,
                      GqlSuccessCallback onSuccess, GqlErrorCallback onError,
                      int timeoutMs)
{
    const QJsonObject op{
        {QStringLiteral("operationName"), QString::fromLatin1(operation.name)},
        {QStringLiteral("variables"), variables},
        {QStringLiteral("extensions"),
         QJsonObject{
             {QStringLiteral("persistedQuery"),
              QJsonObject{{QStringLiteral("version"), 1},
                          {QStringLiteral("sha256Hash"),
                           QString::fromLatin1(operation.sha256)}}}}},
    };
    const QByteArray body =
        QJsonDocument(QJsonArray{op}).toJson(QJsonDocument::Compact);
    executeBody(body, gqlToken, std::move(onSuccess), std::move(onError),
                QString::fromLatin1(operation.name), timeoutMs);
}

void executeInline(const QString &operationName, const QString &queryText,
                   const QJsonObject &variables, const QString &gqlToken,
                   GqlSuccessCallback onSuccess, GqlErrorCallback onError,
                   int timeoutMs)
{
    QJsonObject op{
        {QStringLiteral("query"), queryText},
        {QStringLiteral("variables"), variables},
    };
    if (!operationName.isEmpty())
    {
        op.insert(QStringLiteral("operationName"), operationName);
    }
    const QByteArray body =
        QJsonDocument(QJsonArray{op}).toJson(QJsonDocument::Compact);
    executeBody(body, gqlToken, std::move(onSuccess), std::move(onError),
                operationName, timeoutMs);
}

}  // namespace chatterino::LimerinoAuth::gql
