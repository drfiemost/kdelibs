/*
    Copyright (C) 2006 Apple Computer, Inc.
                  2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2007 Rob Buis <buis@kde.org>

    This file is part of the WebKit project

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
#include <wtf/Platform.h>

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"

/*#include "AtomicString.h"
#include "Console.h"
#include "DOMWindow.h"*/
//#include "Document.h"
#include "xml/Document.h"
//#include "EventListener.h"
#include "dom/dom2_events.h"
/*#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"*/
#include "SVGSVGElement.h"
/*#include "SMILTimeContainer.h"
#include "XMLTokenizer.h"*/
#include "kjs_proxy.h"
#include "khtml_part.h"

namespace WebCore {

SVGDocumentExtensions::SVGDocumentExtensions(Document* doc)
    : m_doc(doc)
{
}

SVGDocumentExtensions::~SVGDocumentExtensions()
{
    /*deleteAllValues(m_pendingResources);*/
    deleteAllValues(m_elementInstances);
}

EventListener* SVGDocumentExtensions::createSVGEventListener(const DOMString& functionName, const DOMString& code, DOM::NodeImpl* node)
{
    /*if (Frame* frame = m_doc->frame())
        if (frame->scriptProxy()->isEnabled())
            return frame->scriptProxy()->createSVGEventHandler(functionName, code, node);*/
    if (!m_doc || !m_doc->part())
        return 0;
    kDebug() << "create listener: (" << code << functionName << node << ")" << endl;
    return m_doc->part()->createHTMLEventListener(code.string(), functionName.string(), node, true/*svg*/);
}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement* element)
{
    Q_UNUSED(element);
    /*m_timeContainers.add(element);*/
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement* element)
{
    Q_UNUSED(element);
    /*m_timeContainers.remove(element);*/
}

void SVGDocumentExtensions::startAnimations()
{
    // FIXME: Eventually every "Time Container" will need a way to latch on to some global timer
    // starting animations for a document will do this "latching"
#if ENABLE(SVG_ANIMATION)
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->timeContainer()->begin();
#endif
}

void SVGDocumentExtensions::pauseAnimations()
{
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->pauseAnimations();
}

void SVGDocumentExtensions::unpauseAnimations()
{
    HashSet<SVGSVGElement*>::iterator end = m_timeContainers.end();
    for (HashSet<SVGSVGElement*>::iterator itr = m_timeContainers.begin(); itr != end; ++itr)
        (*itr)->unpauseAnimations();
}

void SVGDocumentExtensions::reportWarning(const String& message)
{
    Q_UNUSED(message);
    /*if (Frame* frame = m_doc->frame())
        frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, "Warning: " + message, m_doc->tokenizer() ? m_doc->tokenizer()->lineNumber() : 1, String());*/
}

void SVGDocumentExtensions::reportError(const String& message)
{
    Q_UNUSED(message);
    /*if (Frame* frame = m_doc->frame())
        frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, "Error: " + message, m_doc->tokenizer() ? m_doc->tokenizer()->lineNumber() : 1, String());*/
}

void SVGDocumentExtensions::addPendingResource(const AtomicString& id, SVGStyledElement* obj)
{
    ASSERT(obj);
    Q_UNUSED(obj);

    if (id.isEmpty())
        return;

    /*if (m_pendingResources.contains(id))
        m_pendingResources.get(id)->add(obj);
    else {
        HashSet<SVGStyledElement*>* set = new HashSet<SVGStyledElement*>();
        set->add(obj);

        m_pendingResources.add(id, set);
    }*/
}

bool SVGDocumentExtensions::isPendingResource(const AtomicString& id) const
{
    Q_UNUSED(id);
    /*if (id.isEmpty())
        return false;

    return m_pendingResources.contains(id);*/
	ASSERT(false);
	return false;
}

std::unique_ptr<HashSet<SVGStyledElement*> > SVGDocumentExtensions::removePendingResource(const AtomicString& id)
{
    Q_UNUSED(id);
    /*ASSERT(m_pendingResources.contains(id));

    std::unique_ptr<HashSet<SVGStyledElement*> > set(m_pendingResources.get(id));
    m_pendingResources.remove(id);
    return set;*/
	ASSERT(false);
	return std::unique_ptr<HashSet<SVGStyledElement*> >();
}

void SVGDocumentExtensions::mapInstanceToElement(SVGElementInstance* instance, SVGElement* element)
{
    ASSERT(instance);
    ASSERT(element);

    if (m_elementInstances.contains(element))
        m_elementInstances.get(element)->add(instance);
    else {
        HashSet<SVGElementInstance*>* set = new HashSet<SVGElementInstance*>();
        set->add(instance);

        m_elementInstances.add(element, set);
    }
}

void SVGDocumentExtensions::removeInstanceMapping(SVGElementInstance* instance, SVGElement* element)
{
    ASSERT(instance);

    if (!m_elementInstances.contains(element))
        return;

    m_elementInstances.get(element)->remove(instance);
}

HashSet<SVGElementInstance*>* SVGDocumentExtensions::instancesForElement(SVGElement* element) const
{
    ASSERT(element);
    return m_elementInstances.get(element);
}

}

#endif
