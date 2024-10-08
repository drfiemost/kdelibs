/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
                  2005 Alexander Kellett <lypanov@kde.org>

    This file is part of the KDE project

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

#include "config.h"

#if ENABLE(SVG)
#include "SVGMaskElement.h"

#include "CSSStyleSelector.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderSVGContainer.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGUnitTypes.h"
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>

using namespace std;

namespace WebCore {

SVGMaskElement::SVGMaskElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_maskUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_maskContentUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(this, LengthModeWidth)
    , m_y(this, LengthModeHeight)
    , m_width(this, LengthModeWidth)
    , m_height(this, LengthModeHeight)
{
    // Spec: If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    setXBaseValue(SVGLength(this, LengthModeWidth, "-10%"));
    setYBaseValue(SVGLength(this, LengthModeHeight, "-10%"));
  
    // Spec: If the attribute is not specified, the effect is as if a value of "120%" were specified.
    setWidthBaseValue(SVGLength(this, LengthModeWidth, "120%"));
    setHeightBaseValue(SVGLength(this, LengthModeHeight, "120%"));
}

SVGMaskElement::~SVGMaskElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGMaskElement, int, Enumeration, enumeration, MaskUnits, maskUnits, SVGNames::maskUnitsAttr, m_maskUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGMaskElement, int, Enumeration, enumeration, MaskContentUnits, maskContentUnits, SVGNames::maskContentUnitsAttr, m_maskContentUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGMaskElement, SVGLength, Length, length, X, x, SVGNames::xAttr, m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGMaskElement, SVGLength, Length, length, Y, y, SVGNames::yAttr, m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGMaskElement, SVGLength, Length, length, Width, width, SVGNames::widthAttr, m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGMaskElement, SVGLength, Length, length, Height, height, SVGNames::heightAttr, m_height)

void SVGMaskElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::maskUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMaskUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setMaskUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::maskContentUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMaskContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setMaskContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGMaskElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (!m_masker)
        return;

    if (attrName == SVGNames::maskUnitsAttr || attrName == SVGNames::maskContentUnitsAttr ||
        attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
        SVGURIReference::isKnownAttribute(attrName) ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName))
        m_masker->invalidate();
}

void SVGMaskElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!m_masker)
        return;

    m_masker->invalidate();
}

std::unique_ptr<ImageBuffer> SVGMaskElement::drawMaskerContent(const FloatRect& targetRect, FloatRect& maskDestRect) const
{    
    // Determine specified mask size
    float xValue;
    float yValue;
    float widthValue;
    float heightValue;

    if (maskUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        xValue = x().valueAsPercentage() * targetRect.width();
        yValue = y().valueAsPercentage() * targetRect.height();
        widthValue = width().valueAsPercentage() * targetRect.width();
        heightValue = height().valueAsPercentage() * targetRect.height();
    } else {
        xValue = x().value();
        yValue = y().value();
        widthValue = width().value();
        heightValue = height().value();
    } 

    IntSize imageSize(lroundf(widthValue), lroundf(heightValue));
    clampImageBufferSizeToViewport(document()->renderer(), imageSize);

    if (imageSize.width() < static_cast<int>(widthValue))
        widthValue = imageSize.width();

    if (imageSize.height() < static_cast<int>(heightValue))
        heightValue = imageSize.height();

    std::unique_ptr<ImageBuffer> maskImage = ImageBuffer::create(imageSize, false);
    if (!maskImage.get())
        return maskImage;

    maskDestRect = FloatRect(xValue, yValue, widthValue, heightValue);
    if (maskUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        maskDestRect.move(targetRect.x(), targetRect.y());

    GraphicsContext* maskImageContext = maskImage->context();
    ASSERT(maskImageContext);

    maskImageContext->save();
    maskImageContext->translate(-xValue, -yValue);

    if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskImageContext->save();
        maskImageContext->scale(FloatSize(targetRect.width(), targetRect.height()));
    }

    // Render subtree into ImageBuffer
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        SVGElement* elem = 0;
        if (n->isSVGElement())
            elem = static_cast<SVGElement*>(n);
        if (!elem || !elem->isStyled())
            continue;

        SVGStyledElement* e = static_cast<SVGStyledElement*>(elem);
        RenderObject* item = e->renderer();
        if (!item)
            continue;

        renderSubtreeToImage(maskImage.get(), item);
    }

    if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        maskImageContext->restore();

    maskImageContext->restore();
    return maskImage;
}
 
RenderObject* SVGMaskElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGContainer* maskContainer = new (arena) RenderSVGContainer(this);
    maskContainer->setDrawsContents(false);
    return maskContainer;
}

SVGResource* SVGMaskElement::canvasResource()
{
    if (!m_masker)
        m_masker = SVGResourceMasker::create(this);
    return m_masker.get();
}

}

#endif // ENABLE(SVG)
