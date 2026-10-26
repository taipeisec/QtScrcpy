#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#ifdef Q_OS_LINUX
#include <QIcon>
#endif
#include <QSurfaceFormat>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTranslator>
#include <QDateTime>

#include "config.h"
#include "dialog.h"
#include "mousetap/mousetap.h"
#include "singleinstance.h"
#include "ui/videoform.h"
#include "groupcontroller/groupcontroller.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QRandomGenerator>

static Dialog *g_mainDlg = Q_NULLPTR;
static QtMessageHandler g_oldMessageHandler = Q_NULLPTR;
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void installTranslator();

static QtMsgType g_msgType = QtInfoMsg;
QtMsgType covertLogLevel(const QString &logLevel);

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("QtScrcpy");
    QCoreApplication::setApplicationName("QtScrcpy");

    const QString executableDir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
#ifdef Q_OS_MACOS
    const QString repositoryRoot = QDir(executableDir).absoluteFilePath("../../../../../../");
#else
    const QString repositoryRoot = QDir(executableDir).absoluteFilePath("../../..");
#endif
    const QDir repositoryDir(repositoryRoot);
    const QString defaultKeymapPath = repositoryDir.exists("keymap")
        ? repositoryDir.filePath("keymap")
        : QDir(executableDir).filePath("keymap");
    const QString defaultConfigPath = QFileInfo(repositoryDir.filePath("config/config.ini")).isFile()
        ? repositoryDir.filePath("config")
        : QDir(executableDir).filePath("config");

    // Keep packaged defaults separate from mutable user data. Development
    // builds use the repository templates, while release packages use files
    // beside the executable. AppImage AppRun may override these paths.
    if (qgetenv("QTSCRCPY_DEFAULT_KEYMAP_PATH").isEmpty()) {
        qputenv("QTSCRCPY_DEFAULT_KEYMAP_PATH", defaultKeymapPath.toLocal8Bit());
    }
    if (qgetenv("QTSCRCPY_DEFAULT_CONFIG_PATH").isEmpty()) {
        qputenv("QTSCRCPY_DEFAULT_CONFIG_PATH", defaultConfigPath.toLocal8Bit());
    }

    // set env
#ifdef Q_OS_WIN32
    qputenv("QTSCRCPY_ADB_PATH", "../../../QtScrcpy/QtScrcpyCore/src/third_party/adb/win/adb.exe");
    qputenv("QTSCRCPY_SERVER_PATH", "../../../QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server");
#endif

#ifdef Q_OS_MACOS
    qputenv("QTSCRCPY_ADB_PATH", "../../../../../../QtScrcpy/QtScrcpyCore/src/third_party/adb/mac/adb");
    qputenv("QTSCRCPY_SERVER_PATH", "../../../../../../QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server");
#endif

#ifdef Q_OS_LINUX
    // Only set environment variables if they are not already set (e.g., by AppImage AppRun)
    if (qgetenv("QTSCRCPY_ADB_PATH").isEmpty()) {
        qputenv("QTSCRCPY_ADB_PATH", "../../../QtScrcpy/QtScrcpyCore/src/third_party/adb/linux/adb");
    }
    if (qgetenv("QTSCRCPY_SERVER_PATH").isEmpty()) {
        qputenv("QTSCRCPY_SERVER_PATH", "../../../QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server");
    }
#endif

    g_msgType = covertLogLevel(Config::getInstance().getLogLevel());

    // set on QApplication before
    // bug: config path is error on mac
    int opengl = Config::getInstance().getDesktopOpenGL();
    if (0 == opengl) {
        QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    } else if (1 == opengl) {
        QApplication::setAttribute(Qt::AA_UseOpenGLES);
    } else if (2 == opengl) {
        QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    }

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif

    QSurfaceFormat varFormat = QSurfaceFormat::defaultFormat();
    varFormat.setVersion(2, 0);
    varFormat.setProfile(QSurfaceFormat::NoProfile);
    /*
    varFormat.setSamples(4);
    varFormat.setAlphaBufferSize(8);
    varFormat.setBlueBufferSize(8);
    varFormat.setRedBufferSize(8);
    varFormat.setGreenBufferSize(8);
    varFormat.setDepthBufferSize(24);
    */
    QSurfaceFormat::setDefaultFormat(varFormat);

    g_oldMessageHandler = qInstallMessageHandler(myMessageOutput);
    QApplication a(argc, argv);

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("QtScrcpy Display & Control Engine");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption serialOpt(QStringList() << "s" << "serial", "Target device serial number", "serial");
    QCommandLineOption titleOpt("window-title", "Custom window title", "title");
    QCommandLineOption xOpt("window-x", "Window X position", "x");
    QCommandLineOption yOpt("window-y", "Window Y position", "y");
    QCommandLineOption wOpt("window-width", "Window width", "width");
    QCommandLineOption hOpt("window-height", "Window height", "height");
    QCommandLineOption roleOpt("role", "Device role: master, slave, independent", "role", "independent");
    QCommandLineOption framelessOpt("frameless", "Enable frameless window");
    QCommandLineOption alwaysTopOpt(QStringList() << "stay-on-top" << "always-top", "Stay on top");
    QCommandLineOption maxSizeOpt("max-size", "Max video size", "size", "0");
    QCommandLineOption bitRateOpt(QStringList() << "b" << "bit-rate", "Video bitrate in bps", "bitrate", "4000000");
    QCommandLineOption maxFpsOpt("max-fps", "Max frame rate", "fps", "0");
    QCommandLineOption noReverseOpt("no-reverse", "Do not use adb reverse");
    QCommandLineOption closeScreenOpt("close-screen", "Close device screen when mirroring");

    parser.addOption(serialOpt);
    parser.addOption(titleOpt);
    parser.addOption(xOpt);
    parser.addOption(yOpt);
    parser.addOption(wOpt);
    parser.addOption(hOpt);
    parser.addOption(roleOpt);
    parser.addOption(framelessOpt);
    parser.addOption(alwaysTopOpt);
    parser.addOption(maxSizeOpt);
    parser.addOption(bitRateOpt);
    parser.addOption(maxFpsOpt);
    parser.addOption(noReverseOpt);
    parser.addOption(closeScreenOpt);

    parser.process(a);

    const QString cliSerial = parser.value(serialOpt).trimmed();
    const bool isCli = !cliSerial.isEmpty();

    // Isolated single instance per serial in CLI mode, or global in GUI mode
    QString instanceKey = QStringLiteral("QtScrcpy");
    if (isCli) {
        instanceKey += QStringLiteral("_") + cliSerial;
    }

    SingleInstance singleInstance(instanceKey);
    if (!singleInstance.isPrimary()) {
        singleInstance.notifyPrimary();
        return 0;
    }

    // Set application icon for Linux (taskbar icon)
#ifdef Q_OS_LINUX
    // Load icon from Qt resource (logo.png is included in res.qrc)
    QIcon appIcon(":/image/tray/logo.png");
    if (!appIcon.isNull()) {
        a.setWindowIcon(appIcon);
    }
#endif

    // windows下通过qmake VERSION变量或者rc设置版本号和应用名称后，这里可以直接拿到
    // mac下拿到的是CFBundleVersion的值
    qDebug() << a.applicationVersion();
    qDebug() << a.applicationName();

    //update version
    QStringList versionList = QCoreApplication::applicationVersion().split(".");
    if (versionList.size() >= 3) {
        QString version = versionList[0] + "." + versionList[1] + "." + versionList[2];
        a.setApplicationVersion(version);
    }

    installTranslator();
#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
    MouseTap::getInstance()->initMouseEventTap();
#endif

    // load style sheet
    QFile file(":/qss/psblack.css");
    if (file.open(QFile::ReadOnly)) {
        QString qss = QLatin1String(file.readAll());
        QPalette palette;
        palette.setColor(QPalette::Window, QColor("#1e1e1e"));
        palette.setColor(QPalette::WindowText, QColor("#ffffff"));
        palette.setColor(QPalette::Base, QColor("#252525"));
        palette.setColor(QPalette::AlternateBase, QColor("#2d2d2d"));
        palette.setColor(QPalette::Text, QColor("#ffffff"));
        palette.setColor(QPalette::Button, QColor("#252525"));
        palette.setColor(QPalette::ButtonText, QColor("#ffffff"));
        palette.setColor(QPalette::Highlight, QColor("#0078d4"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#6b6b6b"));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6b6b6b"));
        palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#2d2d2d"));
        qApp->setPalette(palette);
        qApp->setStyleSheet(qss);
        file.close();
    }

    qsc::AdbProcess::setAdbPath(Config::getInstance().getAdbPath());

    if (isCli) {
        QString serverLocalPath = QString::fromLocal8Bit(qgetenv("QTSCRCPY_SERVER_PATH"));
        QFileInfo serverInfo(serverLocalPath);
        if (serverLocalPath.isEmpty() || !serverInfo.isFile()) {
            serverLocalPath = QCoreApplication::applicationDirPath() + "/scrcpy-server";
        }

        int winX = parser.isSet(xOpt) ? parser.value(xOpt).toInt() : -1;
        int winY = parser.isSet(yOpt) ? parser.value(yOpt).toInt() : -1;
        int winW = parser.isSet(wOpt) ? parser.value(wOpt).toInt() : -1;
        int winH = parser.isSet(hOpt) ? parser.value(hOpt).toInt() : -1;
        QString winTitle = parser.value(titleOpt);
        QString role = parser.value(roleOpt).toLower();
        bool frameless = parser.isSet(framelessOpt);
        bool stayOnTop = parser.isSet(alwaysTopOpt);

        QObject::connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceConnected,
                &a, [cliSerial, winTitle, winX, winY, winW, winH, role, frameless, stayOnTop](bool success, const QString &serial, const QString &deviceName, const QSize &size) {
            Q_UNUSED(deviceName);
            if (serial != cliSerial) return;
            if (!success) {
                qCritical() << "[CLI] Failed to connect device:" << serial;
                qApp->exit(1);
                return;
            }

            auto videoForm = new VideoForm(frameless, Config::getInstance().getSkin(), false, 0);
            videoForm->setSerial(serial);

            auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
            if (device) {
                device->setUserData(static_cast<void*>(videoForm));
                device->registerDeviceObserver(videoForm);
            }

            videoForm->showFPS(false);
            if (stayOnTop) {
                videoForm->staysOnTop(true);
            }

            QString title = winTitle;
            if (title.isEmpty()) {
                title = QStringLiteral("QtScrcpy - ") + serial;
            }
            videoForm->setWindowTitle(title);
            videoForm->updateShowSize(size);

            if (winW > 0 && winH > 0) {
                videoForm->resize(winW, winH);
                videoForm->setGeometry(winX, winY, winW, winH);
            }

#ifdef Q_OS_WIN32
            QTimer::singleShot(100, videoForm, [videoForm]() {
                if (videoForm) videoForm->show();
            });
#else
            videoForm->show();
#endif

            if (role != "independent") {
                GroupController::instance().addDevice(serial);
            }
        });

        QObject::connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceDisconnected,
                &a, [cliSerial](const QString &serial) {
            if (serial == cliSerial) {
                GroupController::instance().removeDevice(serial);
                auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
                if (device) {
                    auto data = device->getUserData();
                    device->setUserData(nullptr);
                    if (data) {
                        auto form = static_cast<VideoForm*>(data);
                        form->close();
                        delete form;
                    }
                }
                qInfo() << "[CLI] Device disconnected, quitting:" << serial;
                qApp->quit();
            }
        });

        qsc::DeviceParams params;
        params.serial = cliSerial;
        params.maxSize = parser.value(maxSizeOpt).toUShort();
        params.bitRate = parser.value(bitRateOpt).toUInt();
        params.maxFps = parser.value(maxFpsOpt).toUInt();
        params.closeScreen = parser.isSet(closeScreenOpt);
        params.useReverse = !parser.isSet(noReverseOpt);
        params.display = true;
        params.renderExpiredFrames = Config::getInstance().getRenderExpiredFrames();
        params.stayAwake = true;
        params.serverLocalPath = serverLocalPath;
        params.serverRemotePath = Config::getInstance().getServerPath();
        params.pushFilePath = Config::getInstance().getPushFilePath();
        params.logLevel = Config::getInstance().getLogLevel();
        params.codecOptions = Config::getInstance().getCodecOptions();
        params.codecName = Config::getInstance().getCodecName();
        params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;
        params.decodeMode = 0;

        qsc::IDeviceManage::getInstance().connectDevice(params);
    } else {
        g_mainDlg = new Dialog {};
        g_mainDlg->show();
        QObject::connect(&singleInstance, &SingleInstance::activateRequested, g_mainDlg, &Dialog::bringToFront);
    }

    qInfo() << QObject::tr("This software is completely open source and free. Use it at your own risk. You can download it at the "
            "following address:");
    qInfo() << QString("QtScrcpy %1 <https://github.com/barry-ran/QtScrcpy>").arg(QCoreApplication::applicationVersion());

    int ret = a.exec();
    if (g_mainDlg) {
        delete g_mainDlg;
        g_mainDlg = Q_NULLPTR;
    }

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
    MouseTap::getInstance()->quitMouseEventTap();
#endif
    return ret;
}

void installTranslator()
{
    static QTranslator translator;
    QLocale locale;
    QLocale::Language language = locale.language();

    if (Config::getInstance().getLanguage() == "zh_CN") {
        language = QLocale::Chinese;
    } else if (Config::getInstance().getLanguage() == "en_US") {
        language = QLocale::English;
    } else if (Config::getInstance().getLanguage() == "ja_JP") {
        language = QLocale::Japanese;
    } else if (Config::getInstance().getLanguage() == "ko_KR") {
        language = QLocale::Korean;
    }

    QString languagePath = ":/i18n/";
    switch (language) {
    case QLocale::Chinese:
        languagePath += "zh_CN.qm";
        break;
    case QLocale::Japanese:
        languagePath += "ja_JP.qm";
        break;
    case QLocale::Korean:
        languagePath += "ko_KR.qm";
        break;
    case QLocale::English:
    default:
        languagePath += "en_US.qm";
        break;
    }

    auto loaded = translator.load(languagePath);
    if (!loaded) {
        qWarning() << "Failed to load translation file:" << languagePath;
    }
    qApp->installTranslator(&translator);
}

QtMsgType covertLogLevel(const QString &logLevel)
{
    if ("debug" == logLevel) {
        return QtDebugMsg;
    }

    if ("info" == logLevel) {
        return QtInfoMsg;
    }

    if ("warn" == logLevel) {
        return QtWarningMsg;
    }

    if ("error" == logLevel) {
        return QtCriticalMsg;
    }

#ifdef QT_NO_DEBUG
    return QtInfoMsg;
#else
    return QtDebugMsg;
#endif
}

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString outputMsg;
    
#ifdef ENABLE_DETAILED_LOGS
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    if (context.file && context.line > 0) {
        QString fileName = QString::fromUtf8(context.file);

        int lastSlash = fileName.lastIndexOf('/');
        if (lastSlash >= 0) {
            fileName = fileName.mid(lastSlash + 1);
        }
        lastSlash = fileName.lastIndexOf('\\');
        if (lastSlash >= 0) {
            fileName = fileName.mid(lastSlash + 1);
        }
        
        outputMsg = QString("[ %1 %2: %3 ] %4").arg(timestamp).arg(fileName).arg(context.line).arg(msg);
    } else {
        outputMsg = QString("[%1] %2").arg(timestamp).arg(msg);
    }

    switch (type) {
    case QtDebugMsg:
        outputMsg.prepend("[debug] ");
        break;
    case QtInfoMsg:
        outputMsg.prepend("[info] ");
        break;
    case QtWarningMsg:
        outputMsg.prepend("[warring] ");
        break;
    case QtCriticalMsg:
        outputMsg.prepend("[critical] ");
        break;
    case QtFatalMsg:
        outputMsg.prepend("[fatal] ");
        break;
    }

    fprintf(stderr, "%s\n", outputMsg.toUtf8().constData());
#else
    outputMsg = msg;
    if (g_oldMessageHandler) {
        g_oldMessageHandler(type, context, outputMsg);
    }
#endif

    // Is Qt log level higher than warning?
    float fLogLevel = g_msgType;
    if (QtInfoMsg == g_msgType) {
        fLogLevel = QtDebugMsg + 0.5f;
    }
    float fLogLevel2 = type;
    if (QtInfoMsg == type) {
        fLogLevel2 = QtDebugMsg + 0.5f;
    }

    if (fLogLevel <= fLogLevel2) {
        if (g_mainDlg && g_mainDlg->isVisible() && !g_mainDlg->filterLog(outputMsg)) {
            g_mainDlg->outLog(outputMsg);
        }
    }

    if (QtFatalMsg == type) {
        //abort();
    }
}
