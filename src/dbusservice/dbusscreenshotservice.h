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

/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -a dbusscreenshotservice -c DBusScreenshotService -l DBusScreenshotService com.deepin.Screenshot.xml
 *
 * qdbusxml2cpp is Copyright (C) 2016 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef DBUSSCREENSHOTSERVICE_H
#define DBUSSCREENSHOTSERVICE_H

#include <QDBusAbstractAdaptor>
#include <QtCore/QObject>
#include <QtDBus/QtDBus>

#include "src/screenshot.h"

QT_BEGIN_NAMESPACE

class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;
QT_END_NAMESPACE

/*
 * Adaptor class for interface com.deepin.Screenshot
 */
class DBusScreenshotService: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.Screenshot")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.deepin.Screenshot\">\n"
"    <method name=\"StartScreenshot\"/>\n"
"    <method name=\"DelayScreenshot\">\n"
"      <arg direction=\"in\" type=\"x\"/>\n"
"   </method>\n"
"    <method name=\"NoNotifyScreenshot\"/>\n"
"    <method name=\"TopWindowScreenshot\"/>\n"
"    <method name=\"FullscreenScreenshot\"/>\n"
"    <method name=\"SavePathScreenshot\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
        "")
public:
    DBusScreenshotService(Screenshot *parent);
    ~DBusScreenshotService();

    void setSingleInstance(bool instance);

    inline Screenshot *parent() const
    { return static_cast<Screenshot *>(QObject::parent()); }

public: // PROPERTIES
public Q_SLOTS: // METHODS
    void StartScreenshot();
Q_SIGNALS: // SIGNALS
private:
    bool m_singleInstance;
};

#endif
