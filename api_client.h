#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>

class QNetworkReply;

struct ApiResult {
    bool ok = false;
    QJsonDocument data;
    QString error;
};

struct StreamResult {
    bool ok = false;
    QString error;
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const QString &baseUrl = QStringLiteral("http://localhost:8000"),
                       QObject *parent = nullptr);

    void setToken(const QString &token);
    QString token() const;

    void get(const QString &path,
             std::function<void(const ApiResult &)> callback,
             bool auth = true);
    void post(const QString &path, const QJsonObject &body,
              std::function<void(const ApiResult &)> callback,
              bool auth = true);
    void put(const QString &path, const QJsonObject &body,
             std::function<void(const ApiResult &)> callback,
             bool auth = true);
    void remove(const QString &path,
                std::function<void(const ApiResult &)> callback,
                bool auth = true);
    void postStream(const QString &path, const QJsonObject &body,
                    std::function<void(const QString &chunk)> onChunk,
                    std::function<void(const StreamResult &)> onComplete,
                    std::function<bool()> alive = nullptr);

private:
    QNetworkReply* makeRequest(const QString &method, const QString &path,
                               const QByteArray &body, bool auth);
    void handleReply(QNetworkReply *reply,
                     std::function<void(const ApiResult &)> callback);

    QString m_baseUrl;
    QString m_token;
    QNetworkAccessManager m_nam;
};

#endif
