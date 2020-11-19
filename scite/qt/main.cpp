/***************************************************************************
 *
 * SciteQt - a port of SciTE to Qt Quick/QML
 *
 * Copyright (C) 2020 by Michael Neuroth
 *
 ***************************************************************************/

#include "ScintillaEditBase.h"

#include <QtQuick/QQuickView>
#include <QGuiApplication>
#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTranslator>

#include "applicationdata.h"
#include "sciteqt.h"

#include <qhtml5file/qhtmlfileaccess.h>

#include "applicationui.hpp"
#include "storageaccess.h"

#ifdef Q_OS_ANDROID
#define _WITH_QDEBUG_REDIRECT
#define _WITH_ADD_TO_LOG
#endif

#include <QDir>
#include <QDateTime>

#include "ILexer.h"
#include "Scintilla.h"
//#include "SciLexer.h"
#include "Lexilla.h"
#include "LexillaLibrary.h"

#ifdef _WITH_QDEBUG_REDIRECT
static qint64 g_iLastTimeStamp = 0;

#include <QDebug>
void PrivateMessageHandler(QtMsgType type, const QMessageLogContext & context, const QString & msg)
{
    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("Debug: %1 (%2:%3, %4)").arg(msg).arg(context.file).arg(context.line).arg(context.function);
        break;
    case QtWarningMsg:
        txt = QString("Warning: %1 (%2:%3, %4)").arg(msg).arg(context.file).arg(context.line).arg(context.function);
        break;
    case QtCriticalMsg:
        txt = QString("Critical: %1 (%2:%3, %4)").arg(msg).arg(context.file).arg(context.line).arg(context.function);
        break;
    case QtFatalMsg:
        txt = QString("Fatal: %1 (%2:%3, %4)").arg(msg).arg(context.file).arg(context.line).arg(context.function);
        break;
    case QtInfoMsg:
        txt = QString("Info: %1 (%2:%3, %4)").arg(msg).arg(context.file).arg(context.line).arg(context.function);
        break;
    }
    AddToLog(txt);
}
#endif

void AddToLog(const QString & msg)
{
#ifdef _WITH_ADD_TO_LOG
    QString sFileName(LOG_NAME);
    //if( !QDir("/sdcard/Texte").exists() )
    //{
    //    sFileName = "mgv_quick_qdebug.log";
    //}
    QFile outFile(sFileName);
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 delta = now - g_iLastTimeStamp;
    g_iLastTimeStamp = now;
    ts << delta << " ";
    ts << msg << endl;
    qDebug() << delta << " " << msg << endl;
    outFile.flush();
#else
    Q_UNUSED(msg)
#endif
}

//QString g_sDebugMsg;

QString CheckForOverrideLanguage(const QStringList & args, const QString & currentLanguage)
{
    for(const QString & arg : args)
    {
        if(arg.toLower().startsWith("--language="))
        {
            return arg.toLower().mid(11);
        }
    }
    return currentLanguage;
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    QQuickStyle::setStyle("Fusion");
    //QQuickStyle::setStyle("Material");
    //QQuickStyle::setStyle("Universal");
#elif defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    //QQuickStyle::setStyle("Default");
#endif

    QApplication app(argc, argv);
    app.setOrganizationName("scintilla.org");
    app.setOrganizationDomain("scintilla.org");
    app.setApplicationName("SciTEQt");

#ifdef _WITH_QDEBUG_REDIRECT
    qInstallMessageHandler(PrivateMessageHandler);
#endif

#ifdef Q_OS_ANDROID
    UnpackFiles();
    qputenv(SCITE_HOME, FILES_DIR);
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    //bool ok = false;
    // copy SciTEUser.properties from sciteqt installation directory to user directory (if not existing)
    QString sInstallationFullPath = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) + QDir::separator() + "SciTEUser.properties";
    QString sTargetFullPath = QDir::toNativeSeparators(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first()) + QDir::separator() + "SciTEUser.properties";
    if( QFile::exists(sInstallationFullPath) && !QFile::exists(sTargetFullPath) )
    {
        /*ok =*/ QFile::copy(sInstallationFullPath, sTargetFullPath);
    }
#endif

    QString sLanguage = QLocale::system().name().mid(0,2).toLower();
    QString sQtLanguage = QLocale::system().name().mid(0,2).toLower();
    if(sLanguage=="pt")
    {
        sLanguage = "pt_PT";
    }
    if(sLanguage=="sw")
    {
        sLanguage = "sw_KE";
    }
    if(sLanguage=="ko")
    {
        sLanguage = "ko_KR";
    }
    if(sLanguage=="zh")
    {
        sLanguage = "zh_s";
        sQtLanguage = "zh_tw";
    }
    // this environment variable is used to replace the language macro in SciTEGlobal.properties:
    // locale.properties=locale.$(SciteQtLanguage).properties
    sLanguage = CheckForOverrideLanguage(app.arguments(), sLanguage);
    qputenv(SCITE_QT_LANGUAGE, sLanguage.toLocal8Bit());

#if defined(Q_OS_ANDROID)
    QTranslator qtTranslator;
    /*bool ok =*/ qtTranslator.load("assets:/files/qt_"+sQtLanguage+".qm");
    app.installTranslator(&qtTranslator);
#endif

    qRegisterMetaType<SCNotification>("SCNotification");
    qRegisterMetaType<SCNotification>("uptr_t");
    qRegisterMetaType<SCNotification>("sptr_t");
    qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    qmlRegisterType<SciTEQt>("org.scintilla.sciteqt", 1, 0, "SciTEQt");
    // need external function to register for mingw, otherwise we get an unresolved external errror when linking
    //qmlRegisterType<ScintillaEditBase>("org.scintilla.scintilla", 1, 0, "ScintillaEditBase");
    RegisterScintillaType();

#ifndef NO_EXTENSIONS
    MultiplexExtension multiExtender;

#ifndef NO_LUA
    multiExtender.RegisterExtension(LuaExtension::Instance());
#endif

#ifndef NO_FILER
    //multiExtender.RegisterExtension(DirectorExtension::Instance());
#endif
#endif

    LexillaSetDefaultDirectory(/*GetSciTEPath(FilePath()).AsUTF8()*/".");
    Scintilla_LinkLexers();
    //Scintilla_RegisterClasses(hInstance);
    LexillaSetDefault([](const char *name) {
        return CreateLexer(name);
    });

#if defined(Q_OS_ANDROID)
    ApplicationUI appui;
#endif

    app.setWindowIcon(QIcon("scite_logo.png"));

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qml/app.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    StorageAccess aStorageAccess;

#if defined(Q_OS_ANDROID)
    QObject::connect(&app, SIGNAL(applicationStateChanged(Qt::ApplicationState)), &appui, SLOT(onApplicationStateChanged(Qt::ApplicationState)));
    QObject::connect(&app, SIGNAL(saveStateRequest(QSessionManager &)), &appui, SLOT(onSaveStateRequest(QSessionManager &)), Qt::DirectConnection);
    QObject::connect(&appui, SIGNAL(requestApplicationQuit()), &app, SLOT(quit())/* , Qt::QueuedConnection*/);
#endif

#if defined(Q_OS_ANDROID)
    ApplicationData data(0, appui.GetShareUtils(), aStorageAccess, &multiExtender, engine);
    QObject::connect(&app, SIGNAL(applicationStateChanged(Qt::ApplicationState)), &data, SLOT(sltApplicationStateChanged(Qt::ApplicationState)));
#else
    ApplicationData data(0, new ShareUtils(), aStorageAccess, &multiExtender, engine);
#endif
    engine.rootContext()->setContextProperty("applicationData", &data);
    engine.rootContext()->setContextProperty("storageAccess", &aStorageAccess);

    QHtmlFileAccess htmlFileAccess(qApp);
    engine.rootContext()->setContextProperty("htmlFileAccess", &htmlFileAccess);

    engine.load(url);

    return app.exec();
}
