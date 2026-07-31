// SPDX-License-Identifier: MIT
// Twitch GQL client for the ported command set.
// - persisted queries (operationName + sha256 registry) and inline queries,
//   exactly as pluginforreference/requests.lua issues them
// - headers: authorization "OAuth <resolver token>", client-id from LimerinoAuth
// - HTTP 200 with a populated errors[] array is treated as FAILURE (rule 9)
// - all requests flow through LimerinoRateLimiter

#pragma once

#include <QJsonObject>
#include <QString>

#include "providers/limerino/gql/PersistedQueries.hpp"

#include <functional>

namespace chatterino::LimerinoAuth::gql {

struct GqlError {
    bool httpFailed = false;
    int httpStatus = 0;
    QString message;  // user-facing, token-free
    QStringList rawErrors;  // errors[] text (redacted upstream)
};

using GqlSuccessCallback = std::function<void(const QJsonObject &data)>;
using GqlErrorCallback = std::function<void(const GqlError &error)>;

// operationName must exist in PersistedQueries' registry; this never invents one.
void executePersisted(const PersistedQuery &operation,
                      const QJsonObject &variables, const QString &gqlToken,
                      GqlSuccessCallback onSuccess, GqlErrorCallback onError,
                      int timeoutMs = 5000);

// Inline (non-persisted) query text, verbatim from the registry.
void executeInline(const QString &operationName, const QString &queryText,
                   const QJsonObject &variables, const QString &gqlToken,
                   GqlSuccessCallback onSuccess, GqlErrorCallback onError,
                   int timeoutMs = 5000);

}  // namespace chatterino::LimerinoAuth::gql
