/* This file is part of the KDE libraries
    Copyright (C) 2003 Stephan Binner <binner@kde.org>
    Copyright (C) 2003 Zack Rusin <zack@kde.org>
    Copyright (C) 2009 Urs Wolfer <uwolfer @ kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KTABBAR_H
#define KTABBAR_H

#include <kdeui_export.h>

#include <QtGui/QTabBar>

/**
 * A QTabBar with extended features.
 *
 * \image html ktabbar.png "KDE Tab Bar"
 */
class KDEUI_EXPORT KTabBar: public QTabBar //krazy:exclude=qclasses
{
    Q_OBJECT

  public:
    /**
     * Creates a new tab bar.
     *
     * @param parent The parent widget.
     */
    explicit KTabBar( QWidget* parent = 0 );

    /**
     * Destroys the tab bar.
     */
    virtual ~KTabBar();

    /**
     * Selects the tab which has a tab header at
     * given @param position.
     *
     * @param position the coordinates of the tab
     */
    int selectTab( const QPoint &position ) const;

  Q_SIGNALS:
    /**
     * A right mouse button click was performed over the tab with the @param index.
     * The signal is emitted on the press of the mouse button.
     */
    void contextMenu( int index, const QPoint& globalPos );
    /**
     * A right mouse button click was performed over the empty area on the tab bar.
     * The signal is emitted on the press of the mouse button.
     */
    void emptyAreaContextMenu( const QPoint& globalPos );

    /**
     * A double left mouse button click was performed over the tab with the @param index.
     * The signal is emitted on the second press of the mouse button, before the release.
     */
    void tabDoubleClicked( int index );
    /**
     * A double left mouse button click was performed over the empty area on the tab bar.
     * The signal is emitted on the second press of the mouse button, before the release.
     */
    void newTabRequest();
    /**
     * A double middle mouse button click was performed over the tab with the @param index.
     * The signal is emitted on the release of the mouse button.
     */
    void mouseMiddleClick( int index );
    void initiateDrag( int );
    void testCanDecode( const QDragMoveEvent*, bool& );
    void receivedDropEvent( int, QDropEvent* );
    /**
     * Used internally by KTabBar's/KTabWidget's middle-click tab moving mechanism.
     * Tells the KTabWidget which owns the KTabBar to move a tab.
     */
    void moveTab( int, int );

#ifndef QT_NO_WHEELEVENT
    void wheelDelta( int );
#endif

  protected:
    virtual void mouseDoubleClickEvent( QMouseEvent *event );
    virtual void mousePressEvent( QMouseEvent *event );
    virtual void mouseMoveEvent( QMouseEvent *event );
    virtual void mouseReleaseEvent( QMouseEvent *event );
#ifndef QT_NO_WHEELEVENT
    virtual void wheelEvent( QWheelEvent *event );
#endif

    virtual void dragEnterEvent( QDragEnterEvent *event );
    virtual void dragMoveEvent( QDragMoveEvent *event );
    virtual void dropEvent( QDropEvent *event );

    virtual void paintEvent( QPaintEvent *event );
    virtual void leaveEvent( QEvent *event );
    virtual QSize tabSizeHint( int index ) const;

  protected Q_SLOTS:
    virtual void activateDragSwitchTab();

  protected:
    virtual void tabLayoutChange();

  private:
    QPoint closeButtonPos( int tabIndex ) const;
    QRect closeButtonRect( int tabIndex ) const;

  private:
    class Private;
    Private* const d;
};

#endif
