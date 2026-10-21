#ifndef HTTPMGR_H
#define HTTPMGR_H
#include "singleton.h"
#include "global.h"
#include <QString>
#include <QUrl>
#include <QObject>
#include <QtQml/qqmlregistration.h>// 新增：宏定义所在头
#include <QNetworkAccessManager>//这是 Qt 里负责发送网络请求的核心类。
#include<QJsonObject> //表示一个 JSON 对象
#include<QJsonDocument> //负责 JSON 对象和字符串之间的转换。
//grtp技术

class QQmlEngine;
class QJSEngine;

class HttpMgr:public QObject,public Singleton<HttpMgr>,public std::enable_shared_from_this<HttpMgr>
{
     Q_OBJECT
     QML_ELEMENT                       //把类型注册进模块（QML 里就叫 HttpMgr）
     QML_SINGLETON
public:
    ~HttpMgr();//析构得public
     static HttpMgr *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // 暴露给 QML 的业务接口，QML 不需要接触枚举和 JSON 细节
    Q_INVOKABLE void getVerifyCode(const QString& email);
    Q_INVOKABLE void registerUser(const QString& username, const QString& email,
                                  const QString& password, const QString& verifyCode);
    Q_INVOKABLE void login(const QString& account, const QString& password);
private:
    friend class Singleton<HttpMgr>;//让基类可以访问其构造函数
    HttpMgr();
    QNetworkAccessManager _manager;//qt原生的管理类
    void postHttp(const QUrl& url, const QJsonObject& json, ReqId req_id, Modules mod);
private slots:
    void slot_http_finish(ReqId id,QString res,ErrorCode err,Modules mod);
signals:
    void sig_http_finish(ReqId id,QString res,ErrorCode err,Modules mod);
    void sig_reg_mod_finish(ReqId id,QString res,ErrorCode err);
    void sig_login_mod_finish(ReqId id,QString res,ErrorCode err);
};

#endif // HTTPMGR_H
