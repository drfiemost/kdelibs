/* This file is part of the KDE project
 * Copyright (C) 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 * Copyright (C) 2007 Olaf Schmidt <ojschmidt@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "kcolorspaces.h"
#include "kcolorhelpers_p.h"

#include <QColor>

#include <math.h>

using namespace KColorSpaces;

static inline qreal wrap(qreal a, qreal d = 1.0)
{
    qreal r = fmod(a, d);
    return (r < 0.0 ? d + r : (r > 0.0 ? r : 0.0));
}

///////////////////////////////////////////////////////////////////////////////
// HCY color space

#define HCY_REC 709 // use 709 for now
#if   HCY_REC == 601
static const qreal yc[3] = { 0.299, 0.587, 0.114 };
#elif HCY_REC == 709
static const qreal yc[3] = {0.2126, 0.7152, 0.0722};
#else // use Qt values
static const qreal yc[3] = { 0.34375, 0.5, 0.15625 };
#endif

qreal KHCY::gamma(qreal n)
{
    return pow(normalize(n), 2.2);
}

qreal KHCY::igamma(qreal n)
{
    return pow(normalize(n), 1.0/2.2);
}

qreal KHCY::lumag(qreal r, qreal g, qreal b)
{
    return r*yc[0] + g*yc[1] + b*yc[2];
}

KHCY::KHCY(qreal h_, qreal c_, qreal y_, qreal a_)
{
    h = h_;
    c = c_;
    y = y_;
    a = a_;
}

KHCY::KHCY(const QColor& color)
{
    qreal r = gamma(color.redF());
    qreal g = gamma(color.greenF());
    qreal b = gamma(color.blueF());
    a = color.alphaF();

    // luma component
    y = lumag(r, g, b);

    // hue component
    qreal p = std::max(std::max(r, g), b);
    qreal n = std::min(std::min(r, g), b);
    qreal d = 6.0 * (p - n);
    if (n == p)
        h = 0.0;
    else if (r == p)
        h = ((g - b) / d);
    else if (g == p)
        h = ((b - r) / d) + (1.0 / 3.0);
    else
        h = ((r - g) / d) + (2.0 / 3.0);

    // chroma component
    if (r == g && g == b)
        c = 0.0;
    else
        c = std::max( (y - n) / y, (p - y) / (1 - y) );
}

QColor KHCY::qColor() const
{
    // start with sane component values
    qreal _h = wrap(h);
    qreal _c = normalize(c);
    qreal _y = normalize(y);

    // calculate some needed variables
    qreal _hs = _h * 6.0, th, tm;
    if (_hs < 1.0) {
        th = _hs;
        tm = yc[0] + yc[1] * th;
    }
    else if (_hs < 2.0) {
        th = 2.0 - _hs;
        tm = yc[1] + yc[0] * th;
    }
    else if (_hs < 3.0) {
        th = _hs - 2.0;
        tm = yc[1] + yc[2] * th;
    }
    else if (_hs < 4.0) {
        th = 4.0 - _hs;
        tm = yc[2] + yc[1] * th;
    }
    else if (_hs < 5.0) {
        th = _hs - 4.0;
        tm = yc[2] + yc[0] * th;
    }
    else {
        th = 6.0 - _hs;
        tm = yc[0] + yc[2] * th;
    }

    // calculate RGB channels in sorted order
    qreal tn, to, tp;
    if (tm >= _y) {
        tp = _y + _y * _c * (1.0 - tm) / tm;
        to = _y + _y * _c * (th - tm) / tm;
        tn = _y - (_y * _c);
    }
    else {
        tp = _y + (1.0 - _y) * _c;
        to = _y + (1.0 - _y) * _c * (th - tm) / (1.0 - tm);
        tn = _y - (1.0 - _y) * _c * tm / (1.0 - tm);
    }

    // return RGB channels in appropriate order
    if (_hs < 1.0)
        return QColor::fromRgbF(igamma(tp), igamma(to), igamma(tn), a);
    else if (_hs < 2.0)
        return QColor::fromRgbF(igamma(to), igamma(tp), igamma(tn), a);
    else if (_hs < 3.0)
        return QColor::fromRgbF(igamma(tn), igamma(tp), igamma(to), a);
    else if (_hs < 4.0)
        return QColor::fromRgbF(igamma(tn), igamma(to), igamma(tp), a);
    else if (_hs < 5.0)
        return QColor::fromRgbF(igamma(to), igamma(tn), igamma(tp), a);
    else
        return QColor::fromRgbF(igamma(tp), igamma(tn), igamma(to), a);
}

qreal KHCY::luma(const QColor& color)
{
    return lumag(gamma(color.redF()),
                 gamma(color.greenF()),
                 gamma(color.blueF()));
}

// kate: space-indent on; indent-width 4; replace-tabs on; auto-insert-doxygen on;
