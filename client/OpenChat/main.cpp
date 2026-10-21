#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QDebug>

QString gate_url_prefix;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // 从可执行文件同目录的 config.ini 读取 GateServer 地址
    const QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath("config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    const QString gate_host = settings.value("GateServer/host", "localhost").toString();
    const QString gate_port = settings.value("GateServer/port", "8080").toString();
    gate_url_prefix = "http://" + gate_host + ":" + gate_port;

    if (!QFileInfo::exists(configPath)) {
        qWarning() << "config.ini not found, using" << gate_url_prefix;
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("OpenChat", "Main"); // 注意这里的模块名要和你的 CMake 对应
    return app.exec();
}
