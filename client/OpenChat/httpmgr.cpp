#include "httpmgr.h"
#include <QNetworkReply>
#include <QQmlEngine>
#include <QJSEngine>
#include <QDebug>

namespace {
QUrl makeUrl(const char* path)
{
    return QUrl(QString::fromLatin1(kServerBaseUrl) + QString::fromLatin1(path));
}
}

HttpMgr::~HttpMgr()
{

}

HttpMgr *HttpMgr::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    HttpMgr *m = HttpMgr::GetInstance().get();
    QQmlEngine::setObjectOwnership(m, QQmlEngine::CppOwnership);
    return m;
}

HttpMgr::HttpMgr() {
    connect(this,&HttpMgr::sig_http_finish,this,&HttpMgr::slot_http_finish);
}

void HttpMgr::getVerifyCode(const QString& email)
{
    QJsonObject json;
    json["email"] = email;
    postHttp(makeUrl(kGetVerifyCodePath), json, ID_GET_VERIFY_CODE, REGISTERMOD);
}

void HttpMgr::registerUser(const QString& username, const QString& email,
                           const QString& password, const QString& verifyCode)
{
    QJsonObject json;
    json["username"] = username;
    json["email"] = email;
    json["password"] = password;
    json["verify_code"] = verifyCode;
    postHttp(makeUrl(kUserRegisterPath), json, ID_REG_USER, REGISTERMOD);
}

void HttpMgr::login(const QString& account, const QString& password)
{
    QJsonObject json;
    json["account"] = account;
    json["password"] = password;
    postHttp(makeUrl(kUserLoginPath), json, ID_LOGIN_USER, LOGINMOD);
}

void HttpMgr::postHttp(const QUrl& url, const QJsonObject& json, ReqId req_id, Modules mod)
{
    const QByteArray data = QJsonDocument(json).toJson();
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(10000);

    auto self = shared_from_this();
    QNetworkReply *reply = _manager.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, [self, reply, req_id, mod]() {
        //处理错误
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "http error:" << reply->errorString() << reply->readAll();
            emit self->sig_http_finish(req_id, "", ErrorCode::ERR_NETWORK, mod);
            reply->deleteLater();//没用的时候删除
            return;
        }
        //无错误
        const QString res = QString::fromUtf8(reply->readAll());
        //发送信号完成
        emit self->sig_http_finish(req_id, res, ErrorCode::SUCCESS, mod);
        reply->deleteLater();
    });
}

void HttpMgr::slot_http_finish(ReqId id, QString res, ErrorCode err, Modules mod)
{
    if (mod == Modules::REGISTERMOD) {
        emit sig_reg_mod_finish(id, res, err);
    } else if (mod == Modules::LOGINMOD) {
        emit sig_login_mod_finish(id, res, err);
    }
}
