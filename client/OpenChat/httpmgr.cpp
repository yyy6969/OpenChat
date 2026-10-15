#include "httpmgr.h"
#include <QNetworkReply>
#include <QQmlEngine>
#include <QJSEngine>
#include <QDebug>
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

void HttpMgr::PostHttpReg(QUrl url, QJsonObject json, ReqId req_id, Modules mod)
{
    QByteArray data = QJsonDocument(json).toJson();
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader,QByteArray::number(data.length()));
    auto self = shared_from_this();
    QNetworkReply *reply = _manager.post(request,data);
    QObject::connect(reply,&QNetworkReply::finished,[self,reply,req_id,mod](){
        //处理错误
        if(reply->error()!= QNetworkReply::NoError){
            qDebug()<<reply->errorString();
            emit  self->sig_http_finish(req_id,"",ErrorCode::ERR_NETWORK,mod);
            reply->deleteLater();//没用的时候删除
            return;
        }
        //无错误
        QString res = reply->readAll();
        //发送信号完成
         emit  self->sig_http_finish(req_id,res,ErrorCode::SUCCESS,mod);
        reply->deleteLater();
         return;
    });
}

void HttpMgr::slot_http_finish(ReqId id, QString res, ErrorCode err, Modules mod)
{
    if(mod == Modules::REGISTERMOD){
        emit sig_reg_mod_finish(id,res,err);
    }
}
