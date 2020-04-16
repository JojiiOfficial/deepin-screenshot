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

#include "mainwindow.h"

#include <QDesktopWidget>
#include <QPainter>
#include <QFileDialog>
#include <QClipboard>
#include <QAction>
#include <QMap>
#include <QStyleFactory>
#include <QShortcut>
#include <QKeySequence>
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDBusInterface>
#include <QDir>
#include <DWindowManagerHelper>
#include <DForeignWindow>
#include <QX11Info>

#include <DApplication>

//The deepin-tool-kit didn't update...
//#include <DDesktopServices>

//DCORE_USE_NAMESPACE

#include "src/utils/screenutils.h"
#include "src/utils/tempfile.h"
#include <iostream>

DWIDGET_USE_NAMESPACE

#define QT_NO_DEBUG_OUTPUT

namespace {
    const int RECORD_MIN_SIZE = 10;
    const int SPACING = 10;
    const int TOOLBAR_Y_SPACING = 8;
    const int CURSOR_WIDTH = 8;
    const int CURSOR_HEIGHT = 18;
    const int INDICATOR_WIDTH =  59;
    const qreal RESIZEPOINT_WIDTH = 15;
}

MainWindow::MainWindow(QWidget *parent) : QLabel(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::X11BypassWindowManagerHint);

    installEventFilter(this);

    connect(this, &MainWindow::releaseEvent, this, [=]{
        m_keyboardReleased = true;
        m_keyboardGrabbed =  windowHandle()->setKeyboardGrabEnabled(false);
        removeEventFilter(this);
    });

    connect(this, &MainWindow::hideScreenshotUI, this, &MainWindow::hide);
}

MainWindow::~MainWindow() {}

void MainWindow::initOriginUI() {
    this->setFocus();
    setMouseTracking(true);

    QPoint curPos = this->cursor().pos();

    const qreal ratio = devicePixelRatioF();
    m_swUtil = DScreenWindowsUtil::instance(curPos);
    m_screenNum =  m_swUtil->getScreenNum();
    m_backgroundRect = m_swUtil->backgroundRect();
    m_backgroundRect = QRect(m_backgroundRect.topLeft() / ratio, m_backgroundRect.size());

    move(m_backgroundRect.topLeft() * ratio);
    this->setFixedSize(m_backgroundRect.size());
    initBackground();

    m_sizeTips = new TopTips(this);
    m_sizeTips->hide();
    m_zoomIndicator = new ZoomIndicator(this);
    m_zoomIndicator->hide();

    m_isFirstDrag = false;
    m_isFirstMove = false;
    m_isFirstPressButton = false;
    m_isFirstReleaseButton = false;

    m_recordX = 0;
    m_recordY = 0;
    m_recordWidth = 0;
    m_recordHeight = 0;

    qreal ration =  this->devicePixelRatioF();
    QIcon icon(":/image/icons/resize_handle_big.svg");
    m_resizeBigPix = icon.pixmap(QSize(RESIZEPOINT_WIDTH,RESIZEPOINT_WIDTH));
    m_resizeBigPix.setDevicePixelRatio(ration);

    m_dragRecordX = -1;
    m_dragRecordY = -1;

    m_needDrawSelectedPoint = false;
    m_mouseStatus = ShotMouseStatus::Shoting;

    m_selectAreaName = "";

    m_isShapesWidgetExist = false;

}

void MainWindow::initSecondUI() {
    for (auto wid : DWindowManagerHelper::instance()->currentWorkspaceWindowIdList()) {
        if (wid == winId()) continue;

        DForeignWindow * window = DForeignWindow::fromWinId(wid);
        if (window) {
            window->deleteLater();
            m_windowRects << window->geometry();
            m_windowNames << window->wmClass();
        }
    }

    m_configSettings =  ConfigSettings::instance();
}

void MainWindow::initDBusInterface() {
    m_controlCenterDBInterface = new DBusControlCenter(this);
    m_notifyDBInterface = new DBusNotify(this);
    m_notifyDBInterface->CloseNotification(0);
    m_hotZoneInterface = new DBusZone(this);
    if (m_hotZoneInterface->isValid())
        m_hotZoneInterface->asyncCall("EnableZoneDetected", false);
    m_interfaceExist = true;
}

void MainWindow::initShortcut(){ }

void MainWindow::keyPressEvent(QKeyEvent *keyEvent) {
    if (keyEvent->key() == Qt::Key_Return){
        // Save screenshot on enter
        expressSaveScreenshot();
    }else if (keyEvent->key() == Qt::Key_Escape ) {
        // Exit on esc
        exit(1);
        return;
    }

    QLabel::keyPressEvent(keyEvent);
}

void MainWindow::keyReleaseEvent(QKeyEvent *keyEvent) { }

void MainWindow::mousePressEvent(QMouseEvent *ev) { 
    if (!m_isShapesWidgetExist) {
        m_dragStartX = ev->x();
        m_dragStartY = ev->y();

        if (ev->button() == Qt::RightButton) {
            m_moving = false;
            if (!m_isFirstPressButton) {
                exitApp();
            }

            m_menuController->showMenu(QPoint(mapToGlobal(ev->pos())));
        }

        if (!m_isFirstPressButton) {
            m_isFirstPressButton = true;
        } else if (ev->button() == Qt::LeftButton) {
            m_moving = true;
            m_dragAction = getDirection(ev);

            m_dragRecordX = m_recordX;
            m_dragRecordY = m_recordY;
            m_dragRecordWidth = m_recordWidth;
            m_dragRecordHeight = m_recordHeight;
        }

        m_isPressButton = true;
        m_isReleaseButton = false;
    }
    QLabel::mousePressEvent(ev);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *ev) {
    expressSaveScreenshot();
    QLabel::mouseDoubleClickEvent(ev);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *ev) {
    bool needRepaint = false;

    if (!m_isShapesWidgetExist) {
        m_moving = false;

        if (m_sizeTips->isVisible()) {
            m_sizeTips->updateTips(QPoint(m_recordX, m_recordY),
                                   QString("%1X%2").arg(m_recordWidth).arg(m_recordHeight));
        }

        if (!m_isFirstReleaseButton) {
            m_isFirstReleaseButton = true;

            m_mouseStatus = ShotMouseStatus::Normal;
            m_zoomIndicator->hide();

            updateToolBarPos();
            updateCursor(ev);

            // Record select area name with window name if just click (no drag).
            if (!m_isFirstDrag) {
                for (int i = 0; i < m_windowRects.length(); i++) {
                    int wx = m_windowRects[i].x();
                    int wy = m_windowRects[i].y();
                    int ww = m_windowRects[i].width();
                    int wh = m_windowRects[i].height();
                    int ex = ev->x();
                    int ey = ev->y();
                    if (ex > wx && ex < wx + ww && ey > wy && ey < wy + wh) {
                        m_selectAreaName = m_windowNames[i];
                        break;
                    }
                }
            } else {
                // Make sure record area not too small.
                m_recordWidth = m_recordWidth < RECORD_MIN_SIZE ?
                            RECORD_MIN_SIZE : m_recordWidth;
                m_recordHeight = m_recordHeight < RECORD_MIN_SIZE ?
                            RECORD_MIN_SIZE : m_recordHeight;

                if (m_recordX + m_recordWidth > m_backgroundRect.width()) {
                    m_recordX = m_backgroundRect.width() - m_recordWidth;
                }

                if (m_recordY + m_recordHeight > m_backgroundRect.height()) {
                    m_recordY = m_backgroundRect.height() - m_recordHeight;
                }
            }

            needRepaint = true;
        }

        m_isPressButton = false;
        m_isReleaseButton = true;

        needRepaint = true;
    }

    if (needRepaint) {
        update();
    }

    QLabel::mouseReleaseEvent(ev);
}

void MainWindow::hideEvent(QHideEvent *event) {
    qApp->setOverrideCursor(Qt::ArrowCursor);
    QLabel::hideEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (!m_keyboardGrabbed && this->windowHandle() != NULL) {
        m_keyboardGrabbed = this->windowHandle()->setKeyboardGrabEnabled(true);
    }

    return QLabel::eventFilter(watched, event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *ev) {
    bool needRepaint = false;

    if (!m_isShapesWidgetExist) {
        if (m_recordWidth > 0 && m_recordHeight >0 && !m_needSaveScreenshot && this->isVisible()) {
            m_sizeTips->updateTips(QPoint(m_recordX, m_recordY), QString("%1X%2").arg(m_recordWidth).arg(m_recordHeight));
        }

        if (m_isFirstMove) {
            if (!m_isFirstReleaseButton) {
                QPoint curPos = this->cursor().pos();
                QPoint tmpPos;
                QPoint topLeft = m_backgroundRect.topLeft() * devicePixelRatioF();

                if (curPos.x() + INDICATOR_WIDTH + CURSOR_WIDTH > topLeft.x()
                        + m_backgroundRect.width()) {
                      tmpPos.setX(curPos.x() - INDICATOR_WIDTH);
                } else {
                    tmpPos.setX(curPos.x() + CURSOR_WIDTH);
                }

                if (curPos.y() + INDICATOR_WIDTH > topLeft.y() + m_backgroundRect.height()) {
                    tmpPos.setY(curPos.y() - INDICATOR_WIDTH);
                } else {
                    tmpPos.setY(curPos.y() + CURSOR_HEIGHT);
                }

                m_zoomIndicator->showMagnifier(QPoint(
                    std::max(tmpPos.x() - topLeft.x(), 0),
                    std::max(tmpPos.y() - topLeft.y(), 0)));
            }
        } else {
            m_isFirstMove = true;
            needRepaint = true;
        }

        if (m_isFirstPressButton) {
            if (!m_isFirstReleaseButton) {
                if (m_isPressButton && !m_isReleaseButton) {
                    m_recordX = std::min(m_dragStartX, ev->x());
                    m_recordY = std::min(m_dragStartY, ev->y());
                    m_recordWidth = std::abs(m_dragStartX - ev->x());
                    m_recordHeight = std::abs(m_dragStartY - ev->y());

                    needRepaint = true;
                }
            } else if (m_isPressButton) {
                if (m_mouseStatus != ShotMouseStatus::Wait && m_dragRecordX >= 0
                && m_dragRecordY >= 0) {
                    if (m_dragAction == ResizeDirection::Moving && m_moving) {
                        m_recordX = std::max(std::min(m_dragRecordX + ev->x() - m_dragStartX, m_backgroundRect.width() - m_recordWidth), 1);
                        m_recordY = std::max(std::min(m_dragRecordY + ev->y() - m_dragStartY, m_backgroundRect.height() - m_recordHeight), 1);
                    } else if (m_dragAction == ResizeDirection::TopLeft) {
                        resizeDirection(ResizeDirection::Top, ev);
                        resizeDirection(ResizeDirection::Left, ev);
                    } else if (m_dragAction == ResizeDirection::TopRight) {
                        resizeDirection(ResizeDirection::Top, ev);
                        resizeDirection(ResizeDirection::Right, ev);
                    } else if (m_dragAction == ResizeDirection::BottomLeft) {
                        resizeDirection(ResizeDirection::Bottom, ev);
                        resizeDirection(ResizeDirection::Left, ev);
                    } else if (m_dragAction == ResizeDirection::BottomRight) {
                        resizeDirection(ResizeDirection::Bottom, ev);
                        resizeDirection(ResizeDirection::Right, ev);
                    } else if (m_dragAction == ResizeDirection::Top) {
                        resizeDirection(ResizeDirection::Top, ev);
                    } else if (m_dragAction == ResizeDirection::Bottom) {
                        resizeDirection(ResizeDirection::Bottom, ev);
                    } else if (m_dragAction == ResizeDirection::Left) {
                        resizeDirection(ResizeDirection::Left, ev);
                    } else if (m_dragAction == ResizeDirection::Right) {
                        resizeDirection(ResizeDirection::Right, ev);
                    }

                    needRepaint = true;
                }
            }

            updateCursor(ev);
            int mousePosition =  getDirection(ev);
            bool drawPoint = mousePosition != ResizeDirection::Moving;
            if (drawPoint != m_needDrawSelectedPoint) {
                m_needDrawSelectedPoint = drawPoint;
                needRepaint = true;
            }
        } else {
            const QPoint mousePoint = QCursor::pos();
            for (auto it = m_windowRects.rbegin(); it != m_windowRects.rend(); ++it) {
                if (it->contains(mousePoint)) {
                    m_recordX = it->x() - static_cast<int>(m_backgroundRect.x() * devicePixelRatioF());
                    m_recordY = it->y() - static_cast<int>(m_backgroundRect.y() * devicePixelRatioF());
                    m_recordWidth = it->width();
                    m_recordHeight = it->height();

                    needRepaint = true;
                    break;
                }
            }
        }

        if (m_isPressButton && m_isFirstPressButton) {
            if (!m_isFirstDrag) {
                m_isFirstDrag = true;

                m_selectAreaName = tr("select-area");
            }
        }
    }

    if (needRepaint) 
        update();

    QLabel::mouseMoveEvent(ev);
}

int MainWindow::getDirection(QEvent *event) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

    int cursorX = mouseEvent->x();
    int cursorY = mouseEvent->y();
    int effectiveSpacing = int(SPACING * devicePixelRatioF());

    if (cursorX > m_recordX - effectiveSpacing
            && cursorX < m_recordX + effectiveSpacing
            && cursorY > m_recordY - effectiveSpacing
            && cursorY < m_recordY + effectiveSpacing) {
        // Top-Left corner.
        return ResizeDirection::TopLeft;
    } else if (cursorX > m_recordX + m_recordWidth - effectiveSpacing
               && cursorX < m_recordX + m_recordWidth + effectiveSpacing
               && cursorY > m_recordY + m_recordHeight - effectiveSpacing
               && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
        // Bottom-Right corner.
        return  ResizeDirection::BottomRight;
    } else if (cursorX > m_recordX + m_recordWidth - effectiveSpacing
               && cursorX < m_recordX + m_recordWidth + effectiveSpacing
               && cursorY > m_recordY - effectiveSpacing
               && cursorY < m_recordY + effectiveSpacing) {
        // Top-Right corner.
        return  ResizeDirection::TopRight;
    } else if (cursorX > m_recordX - effectiveSpacing
               && cursorX < m_recordX + effectiveSpacing
               && cursorY > m_recordY + m_recordHeight - effectiveSpacing
               && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
        // Bottom-Left corner.
        return  ResizeDirection::BottomLeft;
    } else if (cursorX > m_recordX - effectiveSpacing
               && cursorX < m_recordX + effectiveSpacing) {
        // Left.
        return ResizeDirection::Left;
    } else if (cursorX > m_recordX + m_recordWidth - effectiveSpacing
               && cursorX < m_recordX + m_recordWidth + effectiveSpacing) {
        // Right.
        return  ResizeDirection::Right;
    } else if (cursorY > m_recordY - effectiveSpacing
               && cursorY < m_recordY + effectiveSpacing) {
        // Top.
        return ResizeDirection::Top;
    } else if (cursorY > m_recordY + m_recordHeight - effectiveSpacing
               && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
        // Bottom.
        return  ResizeDirection::Bottom;
    } else if (cursorX > m_recordX && cursorX < m_recordX + m_recordWidth
               && cursorY > m_recordY && cursorY < m_recordY + m_recordHeight) {
        return ResizeDirection::Moving;
    } else {
        return ResizeDirection::Outting;
    }
}

void MainWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect backgroundRect = QRect(0, 0, m_backgroundRect.width(), m_backgroundRect.height());

    m_backgroundPixmap.setDevicePixelRatio(devicePixelRatioF());
    painter.drawPixmap(backgroundRect, m_backgroundPixmap);

    // Draw background.
    if (!m_isFirstMove) {
        painter.setBrush(QBrush("#000000"));
        painter.setOpacity(0.5);
        painter.drawRect(backgroundRect);
    } else if (m_recordWidth > 0 && m_recordHeight > 0 && !m_drawNothing) {
        QRect frameRect = QRect(m_recordX + 1, m_recordY + 1, m_recordWidth - 2, m_recordHeight - 2);
        // Draw frame.
        if (m_mouseStatus != ShotMouseStatus::Wait) {
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setBrush(QBrush("#000000"));
            painter.setOpacity(0.2);
            painter.setClipping(true);
            painter.setClipRegion(QRegion(backgroundRect).subtracted(QRegion(frameRect)));
            painter.drawRect(backgroundRect);

            painter.setClipRegion(backgroundRect);
            QPen framePen(QColor("#01bdff"));
            framePen.setWidth(2);
            painter.setOpacity(1);
            painter.setBrush(Qt::transparent);
            painter.setPen(framePen);
            painter.drawRect(QRect(frameRect.x(), frameRect.y(), frameRect.width(), frameRect.height()));
            painter.setClipping(false);
        }

        // Draw drag pint.
        if (m_mouseStatus != ShotMouseStatus::Wait && m_needDrawSelectedPoint) {
            painter.setOpacity(1);

            qreal margin =  qreal(RESIZEPOINT_WIDTH / 2);
            qreal paintX = frameRect.x() - margin;
            qreal paintY = frameRect.y() - margin;
            qreal paintWidth = frameRect.x() + frameRect.width() - margin;
            qreal paintHeight = frameRect.y() + frameRect.height() - margin;
            qreal paintHalfWidth = frameRect.x() + frameRect.width()/2 - margin;
            qreal paintHalfHeight = frameRect.y() + frameRect.height()/2 - margin;
            paintSelectedPoint(painter, QPointF(paintX, paintY), m_resizeBigPix);
            paintSelectedPoint(painter, QPointF(paintWidth, paintY), m_resizeBigPix);
            paintSelectedPoint(painter, QPointF(paintX, paintHeight), m_resizeBigPix);
            paintSelectedPoint(painter, QPointF(paintWidth, paintHeight), m_resizeBigPix);

            paintSelectedPoint(painter, QPointF(paintX, paintHalfHeight), m_resizeBigPix);
            paintSelectedPoint(painter, QPointF(paintHalfWidth, paintY), m_resizeBigPix);
            paintSelectedPoint(painter, QPointF(paintWidth, paintHalfHeight), m_resizeBigPix);
            paintSelectedPoint(painter, QPointF(paintHalfWidth, paintHeight), m_resizeBigPix);
        }
    }
}

void MainWindow::initShapeWidget(QString type) {
    m_shapesWidget = new ShapesWidget(this);
    m_shapesWidget->setShiftKeyPressed(m_isShiftPressed);

    if (type != "color")
        m_shapesWidget->setCurrentShape(type);

    m_shapesWidget->show();
    m_shapesWidget->setFixedSize(m_recordWidth - 4, m_recordHeight - 4);
    m_shapesWidget->move(m_recordX + 2, m_recordY + 2);

    updateToolBarPos();
    m_needDrawSelectedPoint = false;
    update();

    connect(m_shapesWidget, &ShapesWidget::reloadEffectImg,
            this, &MainWindow::reloadImage);
    connect(this, &MainWindow::deleteShapes, m_shapesWidget,
            &ShapesWidget::deleteCurrentShape);
    connect(m_shapesWidget, &ShapesWidget::requestScreenshot,
            this, &MainWindow::saveScreenshot);
    connect(m_shapesWidget, &ShapesWidget::requestExit, this, &MainWindow::exitApp);
    connect(this, &MainWindow::unDo, m_shapesWidget, &ShapesWidget::undoDrawShapes);
    connect(this, &MainWindow::saveActionTriggered,
            m_shapesWidget, &ShapesWidget::saveActionTriggered);
    connect(m_shapesWidget, &ShapesWidget::menuNoFocus, this, &MainWindow::activateWindow);
}

void MainWindow::updateCursor(QEvent *event) {
    if (m_mouseStatus == ShotMouseStatus::Normal) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        int cursorX = mouseEvent->x();
        int cursorY = mouseEvent->y();
        int effectiveSpacing = int(SPACING * devicePixelRatioF());

        if (cursorX > m_recordX - effectiveSpacing
                && cursorX < m_recordX + effectiveSpacing
                && cursorY > m_recordY - effectiveSpacing
                && cursorY < m_recordY + effectiveSpacing) {
            // Top-Left corner.
            qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        } else if (cursorX > m_recordX + m_recordWidth - effectiveSpacing
                   && cursorX < m_recordX + m_recordWidth + effectiveSpacing
                   && cursorY > m_recordY + m_recordHeight - effectiveSpacing
                   && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
            // Bottom-Right corner.
            qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        } else if (cursorX > m_recordX + m_recordWidth - effectiveSpacing
                   && cursorX < m_recordX + m_recordWidth + effectiveSpacing
                   && cursorY > m_recordY - effectiveSpacing
                   && cursorY < m_recordY + effectiveSpacing) {
            // Top-Right corner.
            qApp->setOverrideCursor(Qt::SizeBDiagCursor);
        } else if (cursorX > m_recordX - effectiveSpacing
                   && cursorX < m_recordX + effectiveSpacing
                   && cursorY > m_recordY + m_recordHeight - effectiveSpacing
                   && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
            // Bottom-Left corner.
            qApp->setOverrideCursor(Qt::SizeBDiagCursor);
        } else if (cursorX > m_recordX - effectiveSpacing
                   && cursorX < m_recordX + effectiveSpacing) {
            // Left.
            qApp->setOverrideCursor(Qt::SizeHorCursor);
        } else if (cursorX > m_recordX + m_recordWidth - effectiveSpacing
                   && cursorX < m_recordX + m_recordWidth + effectiveSpacing) {
            // Right.
            qApp->setOverrideCursor(Qt::SizeHorCursor);
        } else if (cursorY > m_recordY - effectiveSpacing
                   && cursorY < m_recordY + effectiveSpacing) {
            // Top.
            qApp->setOverrideCursor(Qt::SizeVerCursor);
        } else if (cursorY > m_recordY + m_recordHeight - effectiveSpacing
                   && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
            // Bottom.
            qApp->setOverrideCursor(Qt::SizeVerCursor);
        } else {
            if (m_isPressButton) {
                qApp->setOverrideCursor(Qt::ClosedHandCursor);
            } else if (cursorX >= m_recordX - effectiveSpacing && cursorX <= m_recordX + m_recordWidth + effectiveSpacing
                       && cursorY >= m_recordY - effectiveSpacing && cursorY < m_recordY + m_recordHeight + effectiveSpacing) {
                qApp->setOverrideCursor(Qt::OpenHandCursor);
            } else {
                qApp->setOverrideCursor(Qt::ArrowCursor);
            }
        }
    }
}

void MainWindow::resizeDirection(ResizeDirection direction, QMouseEvent *e) {
    int offsetX = e->x() - m_dragStartX;
    int offsetY = e->y() - m_dragStartY;

    switch (direction) {
    case ResizeDirection::Top: {
        m_recordY = std::max(std::min(m_dragRecordY + offsetY,
                                      m_dragRecordY + m_dragRecordHeight - RECORD_MIN_SIZE), 1);
        m_recordHeight = std::max(std::min(m_dragRecordHeight -
                                           offsetY, m_backgroundRect.height()), RECORD_MIN_SIZE);
        break;
    };
    case ResizeDirection::Bottom: {
        m_recordHeight = std::max(std::min(m_dragRecordHeight + offsetY,
                                           m_backgroundRect.height()), RECORD_MIN_SIZE);
        break;
    };
    case ResizeDirection::Left: {
        m_recordX = std::max(std::min(m_dragRecordX + offsetX,
                                      m_dragRecordX + m_dragRecordWidth - RECORD_MIN_SIZE), 1);
        m_recordWidth = std::max(std::min(m_dragRecordWidth - offsetX,
                                          m_backgroundRect.width()), RECORD_MIN_SIZE);
        break;
    };
    case ResizeDirection::Right: {
        m_recordWidth = std::max(std::min(m_dragRecordWidth + offsetX,
                                          m_backgroundRect.width()), RECORD_MIN_SIZE);
        break;
    };
    default:break;
    }
}

void MainWindow::fullScreenshot() { }
void MainWindow::savePath(const QString &path, const bool noNotify) { }
void MainWindow::saveSpecificedPath(QString path, const bool noNotify) { }


void MainWindow::noNotify() {
    m_controlCenterDBInterface = new DBusControlCenter(this);
    m_hotZoneInterface = new DBusZone(this);
    m_interfaceExist = true;
    m_noNotify = true;

    initOriginUI();
    this->show();
    initSecondUI();
    initShortcut();
}

void MainWindow::topWindow() {
    initOriginUI();
    this->show();
    initSecondUI();
    initDBusInterface();

    if (m_screenNum == 0) {
        m_windowRects  = m_swUtil->windowsRect();
        m_recordX = m_windowRects[0].x();
        m_recordY = m_windowRects[0].y();
        m_recordWidth = m_windowRects[0].width();
        m_recordHeight = m_windowRects[0].height();
    } else {
        m_recordX = m_backgroundRect.x();
        m_recordY = m_backgroundRect.y();
        m_recordWidth = m_backgroundRect.width();
        m_recordHeight = m_backgroundRect.height();
    }

    this->hide();
    emit this->hideScreenshotUI();

    const qreal ratio = this->devicePixelRatioF();
    QRect target( m_recordX * ratio, m_recordY * ratio, m_recordWidth * ratio, m_recordHeight * ratio );

    QPixmap screenShotPix =  m_backgroundPixmap.copy(target);
    m_needSaveScreenshot = true;

    const auto r = saveAction(screenShotPix);
    sendNotify(m_saveIndex, m_saveFileName, r);
}

void MainWindow::expressSaveScreenshot() {
    if (m_specificedPath.isEmpty()) {
        saveScreenshot();
    }
}

void MainWindow::startScreenshot()
{
    initOriginUI();
    m_mouseStatus = ShotMouseStatus::Shoting;
    qApp->setOverrideCursor(setCursorShape("start"));
    this->show();

    initSecondUI();

    initDBusInterface();
    initShortcut();
}

QPixmap MainWindow::getPixmapofRect(const QRect &rect) {
    QRect r(rect.topLeft() * devicePixelRatioF(), rect.size());

    QList<QScreen*> screenList = qApp->screens();
    for (auto it = screenList.constBegin(); it != screenList.constEnd(); ++it) {
        if ((*it)->geometry().contains(r)) {
            return (*it)->grabWindow(m_swUtil->rootWindowId(), rect.x(), rect.y(), rect.width(), rect.height());
        }
    }

    return QPixmap();
}

void MainWindow::initBackground() {
    m_backgroundPixmap = getPixmapofRect(m_backgroundRect);
    m_resultPixmap = m_backgroundPixmap;
    TempFile::instance()->setFullScreenPixmap(m_backgroundPixmap);
}

void MainWindow::shotFullScreen() {
    m_resultPixmap = getPixmapofRect(m_backgroundRect);
}

void MainWindow::shotCurrentImg() {
    if (m_recordWidth == 0 || m_recordHeight == 0)
        return;

    m_needDrawSelectedPoint = false;
    m_drawNothing = true;
    update();

    QEventLoop eventloop1;
    QTimer::singleShot(100, &eventloop1, SLOT(quit()));
    eventloop1.exec();

    shotFullScreen();
    if (m_isShapesWidgetExist) {
        m_shapesWidget->hide();
    }

    this->hide();
    emit hideScreenshotUI();

    const qreal ratio = this->devicePixelRatioF();
    QRect target( m_recordX * ratio,
                  m_recordY * ratio,
                  m_recordWidth * ratio,
                  m_recordHeight * ratio );

    m_resultPixmap = m_resultPixmap.copy(target);
}

void MainWindow::shotImgWidthEffect() {
    if (m_recordWidth == 0 || m_recordHeight == 0)
        return;

    update();
    
    const qreal ratio = devicePixelRatioF();
    const QRect rect(m_shapesWidget->geometry().topLeft() * ratio, m_shapesWidget->geometry().size() * ratio);
    m_resultPixmap = m_backgroundPixmap.copy(rect);
    m_drawNothing = false;
    update();
}

void MainWindow::saveScreenshot() {
    const qreal ratio = this->devicePixelRatioF();
    std::cout << m_recordX*ratio << ";" << m_recordY*ratio << ";" << m_recordWidth*ratio << ";" << m_recordHeight*ratio << std::endl;
    exitApp();
}

bool MainWindow::saveAction(const QPixmap &pix) {
    return true;
}

void MainWindow::sendNotify(SaveAction saveAction, QString saveFilePath, const bool succeed) { }

void MainWindow::reloadImage(QString effect) {
    //*save tmp image file
    shotImgWidthEffect();
    //using namespace utils;
    const int radius = 10;
    QPixmap tmpImg = m_resultPixmap;
    int imgWidth = tmpImg.width();
    int imgHeight = tmpImg.height();

    TempFile *tempFile = TempFile::instance();

    if (effect == "blur") {
        if (!tmpImg.isNull()) {
            tmpImg = tmpImg.scaled(imgWidth/radius, imgHeight/radius, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tmpImg = tmpImg.scaled(imgWidth, imgHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tempFile->setBlurPixmap(tmpImg);
        }
    } else {
        if (!tmpImg.isNull()) {
            tmpImg = tmpImg.scaled(imgWidth/radius, imgHeight/radius,
                                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tmpImg = tmpImg.scaled(imgWidth, imgHeight);
            tempFile->setMosaicPixmap(tmpImg);
        }
    }
}

void MainWindow::onViewShortcut() {
    QRect rect = window()->geometry();
    QPoint pos(rect.x() + rect.width()/2, rect.y() + rect.height()/2);
    Shortcut sc;
    QStringList shortcutString;
    QString param1 = "-j=" + sc.toStr();
    QString param2 = "-p=" + QString::number(pos.x()) + "," + QString::number(pos.y());
    shortcutString << "-b" << param1 << param2;

    QProcess* shortcutViewProc = new QProcess(this);
    shortcutViewProc->startDetached("killall deepin-shortcut-viewer");
    shortcutViewProc->startDetached("deepin-shortcut-viewer", shortcutString);

    connect(shortcutViewProc, SIGNAL(finished(int)), shortcutViewProc, SLOT(deleteLater()));
}

void MainWindow::onHelp() {
    QDBusInterface iface("com.deepin.Manual.Open", "/com/deepin/Manual/Open", "com.deepin.Manual.Open");
    if (iface.isValid()) {
        iface.call("ShowManual", "deepin-screenshot");
        exitApp();
    }
}

void MainWindow::exitApp() {
    if (m_interfaceExist && nullptr != m_hotZoneInterface) {
        if (m_hotZoneInterface->isValid())
            m_hotZoneInterface->asyncCall("EnableZoneDetected",  true);
    }

    qApp->quit();
}

void MainWindow::updateToolBarPos() { }
