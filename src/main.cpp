/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Maintainer: Peng Hui<penghui@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <DApplication>
#include <DLog>
#include <DWidgetUtil>

#include <QObject>
#include <QTranslator>

#include "screenshot.h"
#include "dbusservice/dbusscreenshotservice.h"

DWIDGET_USE_NAMESPACE
#define QT_NO_DEBUG_OUTPUT

// Don't log anything
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg) { }

int main(int argc, char *argv[]) {
    #if defined(STATIC_LIB)
        DWIDGET_INIT_RESOURCE();
    #endif

    qInstallMessageHandler(myMessageOutput);
    DApplication::loadDXcbPlugin();

    DApplication a(argc, argv);
    a.loadTranslator(QList<QLocale>() << QLocale::system());
    a.setApplicationName("deepin-screenshot-selection");
    a.setApplicationVersion("4.0");
    a.setTheme("dark");
    a.setQuitOnLastWindowClosed(true);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    using namespace Dtk::Core;
    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    QCommandLineOption dbusOption(QStringList() << "u" << "dbus", "Start  from dbus.");
    QCommandLineParser cmdParser;
    cmdParser.setApplicationDescription("deepin-screenshot");
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();
    cmdParser.addOption(dbusOption);
    cmdParser.process(a);

    Screenshot w;
    DBusScreenshotService dbusService (&w);
    Q_UNUSED(dbusService);
    // Register Screenshot's dbus service.
    QDBusConnection conn = QDBusConnection::sessionBus();
    if (!conn.registerService("com.deepin.Screenshot") ||
            !conn.registerObject("/com/deepin/Screenshot", &w)) {
         qApp->quit();
        return 0;
    }

    w.startScreenshot();

    return a.exec();
}
