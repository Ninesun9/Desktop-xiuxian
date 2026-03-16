#ifndef SECURE_TRANSPORT_H
#define SECURE_TRANSPORT_H

#include <QByteArray>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QString>
#include <QUrl>

#include "qaesencryption.h"

namespace SecureTransport {

inline QString normalizedBaseUrl()
{
    QString baseUrl = qEnvironmentVariable("XIUXIAN_SERVER_URL").trimmed();
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    return baseUrl;
}

inline QByteArray sharedKey()
{
    return qEnvironmentVariable("XIUXIAN_SHARED_KEY").toUtf8();
}

inline QByteArray apiToken()
{
    return qEnvironmentVariable("XIUXIAN_API_TOKEN").toUtf8();
}

inline bool validateConfig(QString *errorMessage)
{
    if (normalizedBaseUrl().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("XIUXIAN_SERVER_URL 未配置");
        }
        return false;
    }

    if (sharedKey().size() != 16) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("XIUXIAN_SHARED_KEY 必须是 16 字节");
        }
        return false;
    }

    if (apiToken().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("XIUXIAN_API_TOKEN 未配置");
        }
        return false;
    }

    return true;
}

inline QUrl endpointUrl(const QString &path)
{
    return QUrl(normalizedBaseUrl() + path);
}

inline void configureRequest(QNetworkRequest &request, const QString &path)
{
    request.setUrl(endpointUrl(path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("text/plain; charset=utf-8"));
    request.setRawHeader("X-Xiuxian-Token", apiToken());
}

inline QByteArray generateIv()
{
    QByteArray iv(16, Qt::Uninitialized);
    for (int index = 0; index < iv.size(); ++index) {
        iv[index] = static_cast<char>(QRandomGenerator::global()->generate() & 0xff);
    }
    return iv;
}

inline QByteArray encryptPayload(const QByteArray &payload)
{
    const QByteArray iv = generateIv();
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::CBC, QAESEncryption::PKCS7);
    const QByteArray cipherText = encryption.encode(payload, sharedKey(), iv);
    return (iv + cipherText).toBase64();
}

}

#endif // SECURE_TRANSPORT_H