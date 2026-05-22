#include "api_client.h"

#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

ApiClient::ApiClient(const QString &baseUrl, QObject *parent)
    : QObject(parent), m_baseUrl(baseUrl)
{
}

void ApiClient::setToken(const QString &token) { m_token = token; }
QString ApiClient::token() const { return m_token; }

QNetworkReply* ApiClient::makeRequest(const QString &method, const QString &path,
                                       const QByteArray &body, bool auth)
{
    QUrl url(m_baseUrl + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (auth && !m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    if (method == QStringLiteral("GET"))
        return m_nam.get(req);
    if (method == QStringLiteral("POST"))
        return m_nam.post(req, body);
    if (method == QStringLiteral("PUT"))
        return m_nam.put(req, body);
    return m_nam.deleteResource(req);
}

void ApiClient::handleReply(QNetworkReply *reply,
                             std::function<void(const ApiResult &)> callback)
{
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        QByteArray data = reply->readAll();
        ApiResult result;
        if (reply->error() != QNetworkReply::NoError) {
            QJsonObject errObj = QJsonDocument::fromJson(data).object();
            result.error = errObj.value(QStringLiteral("detail")).toString();
            if (result.error.isEmpty())
                result.error = reply->errorString();
        } else {
            result.ok = true;
            result.data = QJsonDocument::fromJson(data);
        }
        reply->deleteLater();
        callback(result);
    });
}

void ApiClient::get(const QString &path,
                     std::function<void(const ApiResult &)> callback, bool auth)
{
    handleReply(makeRequest(QStringLiteral("GET"), path, {}, auth), callback);
}

void ApiClient::post(const QString &path, const QJsonObject &body,
                      std::function<void(const ApiResult &)> callback, bool auth)
{
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    handleReply(makeRequest(QStringLiteral("POST"), path, json, auth), callback);
}

void ApiClient::put(const QString &path, const QJsonObject &body,
                     std::function<void(const ApiResult &)> callback, bool auth)
{
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    handleReply(makeRequest(QStringLiteral("PUT"), path, json, auth), callback);
}

void ApiClient::remove(const QString &path,
                        std::function<void(const ApiResult &)> callback, bool auth)
{
    handleReply(makeRequest(QStringLiteral("DELETE"), path, {}, auth), callback);
}

void ApiClient::postStream(const QString &path, const QJsonObject &body,
                            std::function<void(const QString &chunk)> onChunk,
                            std::function<void(const StreamResult &)> onComplete,
                            std::function<bool()> alive)
{
    QNetworkReply *reply = makeRequest(QStringLiteral("POST"), path,
        QJsonDocument(body).toJson(QJsonDocument::Compact), true);
    auto *accumulator = new QString;

    connect(reply, &QNetworkReply::readyRead, this, [reply, accumulator, onChunk, alive]() {
        if (alive && !alive()) {
            reply->abort();
            reply->deleteLater();
            return;
        }

        *accumulator += QString::fromUtf8(reply->readAll()).remove('\r');

        int idx;
        while ((idx = accumulator->indexOf(QStringLiteral("\n\n"))) != -1) {
            QString event = accumulator->left(idx);
            *accumulator = accumulator->mid(idx + 2);

            QString dataValue;
            for (const QString &line : event.split('\n'))
                if (line.startsWith(QStringLiteral("data: ")))
                    dataValue = line.mid(6);

            if (dataValue.trimmed() == QStringLiteral("[DONE]") || dataValue.isEmpty())
                continue;

            QString chunk;
            if (dataValue.startsWith('{')) {
                QJsonDocument doc = QJsonDocument::fromJson(dataValue.toUtf8());
                if (doc.isObject())
                    chunk = doc.object().value(QStringLiteral("response")).toString();
            } else {
                chunk = dataValue;
            }

            if (!chunk.isEmpty() && onChunk)
                onChunk(chunk);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [reply, accumulator, onChunk, onComplete, alive]() {
        if (alive && !alive()) {
            delete accumulator;
            return;
        }

        StreamResult result;
        if (reply->error() == QNetworkReply::NoError) {
            if (!accumulator->trimmed().isEmpty() &&
                *accumulator != QStringLiteral("[DONE]"))
            {
                QString dataValue;
                for (const QString &line : accumulator->split('\n'))
                    if (line.startsWith(QStringLiteral("data: ")))
                        dataValue = line.mid(6);
                if (!dataValue.isEmpty() && dataValue != QStringLiteral("[DONE]")) {
                    QString chunk;
                    if (dataValue.startsWith('{')) {
                        QJsonDocument doc = QJsonDocument::fromJson(dataValue.toUtf8());
                        if (doc.isObject())
                            chunk = doc.object().value(QStringLiteral("response")).toString();
                    } else {
                        chunk = dataValue;
                    }
                    if (!chunk.isEmpty() && onChunk)
                        onChunk(chunk);
                }
            }
            result.ok = true;
        } else {
            QJsonObject errObj = QJsonDocument::fromJson(accumulator->toUtf8()).object();
            result.error = errObj.value(QStringLiteral("detail")).toString();
            if (result.error.isEmpty())
                result.error = reply->errorString();
        }

        delete accumulator;
        reply->deleteLater();

        if (onComplete)
            onComplete(result);
    });
}
