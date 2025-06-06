/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright 2003 Lars Knoll (knoll@kde.org)
 * Copyright 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Computer, Inc.
 * Copyright (C) 2008 Maksim Orlovich <maksim@kde.org>
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

// #define CSS_DEBUG
// #define TOKEN_DEBUG
#define YYDEBUG 0

#include "cssparser.h"

#include <kdebug.h>
#include <kglobal.h>
#include <kurl.h>

#include "css_valueimpl.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "css_mediaquery.h"
#include "cssproperties.h"
#include "cssvalues.h"

#include <stdlib.h>
#include <assert.h>

using namespace DOM;

// used to promote background: left to left center
#define BACKGROUND_SKIP_CENTER( num ) \
    if ( !pos_ok[ num ] && expected != 1 ) {    \
        pos_ok[num] = true; \
        pos[num] = 0; \
        skip_next = false; \
    }

ValueList::~ValueList()
{
     const int numValues = m_values.size();
     for (int i = 0; i < numValues; ++i)
         if (m_values[i].unit == Value::Function)
             delete m_values[i].function;
}

namespace {
    class ShorthandScope {
    public:
        ShorthandScope(CSSParser* parser, int propId) : m_parser(parser)
        {
            if (!(m_parser->m_inParseShorthand++))
                m_parser->m_currentShorthand = propId;
        }
        ~ShorthandScope()
        {
            if (!(--m_parser->m_inParseShorthand))
                m_parser->m_currentShorthand = 0;
        }

    private:
        CSSParser* m_parser;
    };
}

using namespace DOM;

#if YYDEBUG > 0
extern int cssyydebug;
#endif

extern int cssyyparse( void * parser );

CSSParser *CSSParser::currentParser = 0;

CSSParser::CSSParser( bool strictParsing )
{
#ifdef CSS_DEBUG
    kDebug( 6080 ) << "CSSParser::CSSParser this=" << this;
#endif
    strict = strictParsing;

    parsedProperties = (CSSProperty **) malloc( 32 * sizeof( CSSProperty * ) );
    numParsedProperties = 0;
    maxParsedProperties = 32;

    data = 0;
    valueList = 0;
    rule = 0;
    id = 0;
    important = false;

    m_inParseShorthand = 0;
    m_currentShorthand = 0;
    m_implicitShorthand = false;

    yy_start = 1;

#if YYDEBUG > 0
    cssyydebug = 1;
#endif

}

CSSParser::~CSSParser()
{
    if ( numParsedProperties )
        clearProperties();
    free( parsedProperties );

    delete valueList;

#ifdef CSS_DEBUG
    kDebug( 6080 ) << "CSSParser::~CSSParser this=" << this;
#endif

    free( data );

}

unsigned int CSSParser::defaultNamespace()
{
    if (styleElement && styleElement->isCSSStyleSheet())
        return static_cast<CSSStyleSheetImpl*>(styleElement)->defaultNamespace();
    else
        return anyNamespace;
}

void CSSParser::runParser()
{
    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;
    boundLocalNames.clear();
}

void CSSParser::setupParser(const char *prefix, const DOMString &string, const char *suffix)
{
    unsigned preflen = strlen(prefix);
    unsigned sufflen = strlen(suffix);
    int length = string.length() + preflen + sufflen + 8;

    free(data);

    data = (unsigned short *)malloc( length *sizeof( unsigned short ) );
    for (unsigned i = 0; i < preflen; i++)
        data[i] = prefix[i];

    memcpy(data + preflen, string.unicode(), string.length()*sizeof( unsigned short));

    unsigned start = preflen + string.length();
    unsigned end = start + sufflen;
    for (unsigned i = start; i < end; i++)
        data[i] = suffix[i - start];

    // the flex scanner sometimes give invalid reads for any
    // smaller padding - try e.g. css/invalid-rules-005.html or see #167318
    data[length - 1] = 0;
    data[length - 2] = 0;
    data[length - 3] = 0;
    data[length - 4] = 0;
    data[length - 5] = 0;
    data[length - 6] = 0;
    data[length - 7] = 0;
    data[length - 8] = 0;

    yyTok = -1;
    block_nesting = 0;
    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;
}

void CSSParser::parseSheet( CSSStyleSheetImpl *sheet, const DOMString &string )
{
    styleElement  = sheet;
    styleDocument = 0;

    setupParser("", string, "");

#ifdef CSS_DEBUG
    kDebug( 6080 ) << ">>>>>>> start parsing style sheet";
#endif
    runParser();
#ifdef CSS_DEBUG
    kDebug( 6080 ) << "<<<<<<< done parsing style sheet";
#endif

    delete rule;
    rule = 0;
}

CSSRuleImpl *CSSParser::parseRule( DOM::CSSStyleSheetImpl *sheet, const DOM::DOMString &string )
{
    styleElement  = sheet;
    styleDocument = 0;

    setupParser("@-khtml-rule{", string, "} ");
    runParser();

    CSSRuleImpl *result = rule;
    rule = 0;

    return result;
}

static void addParsedProperties( DOM::CSSStyleDeclarationImpl *declaration, CSSProperty** parsedProperties,
                                 int numProperties)
{
    for ( int i = 0; i < numProperties; i++ ) {
        // Only add properties that have no !important counterpart present
        if (!declaration->getPropertyPriority(parsedProperties[i]->id()) || parsedProperties[i]->isImportant()) {
            declaration->removeProperty(parsedProperties[i]->m_id);
            declaration->values()->append( parsedProperties[i] );
        }
    }
}

bool CSSParser::parseValue( DOM::CSSStyleDeclarationImpl *declaration, int _id, const DOM::DOMString &string,
                            bool _important)
{
#ifdef CSS_DEBUG
    kDebug( 6080 ) << "CSSParser::parseValue: id=" << _id << " important=" << _important
                   << " value='" << string.string() << "'" << endl;
#endif

    styleElement  = declaration->stylesheet();
    styleDocument = 0;

    setupParser("@-khtml-value{", string, "} ");

    id = _id;
    important = _important;

    runParser();

    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties ) {
        ok = true;
        addParsedProperties(declaration, parsedProperties, numParsedProperties);
        numParsedProperties = 0;
    }

    return ok;
}

bool CSSParser::parseDeclaration( DOM::CSSStyleDeclarationImpl *declaration, const DOM::DOMString &string)
{
#ifdef CSS_DEBUG
    kDebug( 6080 ) << "CSSParser::parseDeclaration:"
                    << " value='" << string.string() << "'" << endl;
#endif

    styleElement  = declaration->stylesheet();
    styleDocument = 0;

    setupParser("@-khtml-decls{", string, "} ");
    runParser();

    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties ) {
        ok = true;
        addParsedProperties(declaration, parsedProperties, numParsedProperties);
        numParsedProperties = 0;
    }

    return ok;
}

bool CSSParser::parseMediaQuery(DOM::MediaListImpl* queries, const DOM::DOMString& string)
{
    if (string.isEmpty() || string.isNull()) {
        return true;
    }

    mediaQuery = 0;
    // can't use { because tokenizer state switches from mediaquery to initial state when it sees { token.
    // instead insert one " " (which is S in parser.y)
    setupParser ("@-khtml-mediaquery ", string, "} ");
    runParser();

    bool ok = false;
    if (mediaQuery) {
        ok = true;
        queries->appendMediaQuery(mediaQuery);
        mediaQuery = 0;
    }

    return ok;
}

QList<DOM::CSSSelector*> CSSParser::parseSelectorList(DOM::DocumentImpl* doc, const DOM::DOMString& string)
{
    styleElement  = 0;
    styleDocument = doc;
    selectors.clear();
    setupParser("@-khtml-selectors{", string, "} ");
    runParser();

    // Make sure to detect problems with pseudos, too
    bool ok = true;
    for (int i = 0; i < selectors.size(); ++i) {
        // we need to check not only us, but also other things we're connected to via
        // combinators
        for (DOM::CSSSelector* sel = selectors[i]; sel; sel = sel->tagHistory) {
            if(sel->match == CSSSelector::PseudoClass || sel->match == CSSSelector::PseudoElement) {
                if (sel->pseudoType() == CSSSelector::PseudoOther) {
                    ok = false;
                    break;
                }
            }
        }
    }

    if (!ok) {
        qDeleteAll(selectors);
        selectors.clear();
    }

    return selectors;
}

void CSSParser::addProperty( int propId, CSSValueImpl *value, bool important )
{
    CSSProperty *prop = new CSSProperty;
    prop->m_id = propId;
    prop->setValue( value );
    prop->m_important = important;
    prop->m_implicit = m_implicitShorthand;

    if ( numParsedProperties >= maxParsedProperties ) {
        maxParsedProperties += 32;
        parsedProperties = (CSSProperty **) realloc( parsedProperties,
                                                    maxParsedProperties*sizeof( CSSProperty * ) );
    }
    parsedProperties[numParsedProperties++] = prop;
}

void CSSParser::rollbackParsedProperties(int toNumParsedProperties)
{
    while (numParsedProperties > toNumParsedProperties) {
            --numParsedProperties;
            delete parsedProperties[numParsedProperties];
        }
}

CSSStyleDeclarationImpl *CSSParser::createStyleDeclaration( CSSStyleRuleImpl *rule )
{
    QList<CSSProperty*> *propList = new QList<CSSProperty*>;
    for ( int i = 0; i < numParsedProperties; i++ )
        propList->append( parsedProperties[i] );

    numParsedProperties = 0;
    return new CSSStyleDeclarationImpl(rule, propList);
}

CSSStyleDeclarationImpl *CSSParser::createFontFaceStyleDeclaration( CSSFontFaceRuleImpl *rule )
{
    QList<CSSProperty*> *propList = new QList<CSSProperty*>;
    CSSProperty* overriddenSrcProperty = 0;
    for ( int i = 0; i < numParsedProperties; i++ ) {
        CSSProperty* property = parsedProperties[i];
        int id = property->id();
        if ((id == CSS_PROP_FONT_WEIGHT || id == CSS_PROP_FONT_STYLE || id == CSS_PROP_FONT_VARIANT) && property->value()->isPrimitiveValue()) {
            // change those to a list of values containing a single value, so that we may always cast to a list in the CSSFontSelector.
            CSSValueImpl* value = property->value();
            value->ref();
            property->setValue( new CSSValueListImpl(CSSValueListImpl::Comma) );
            static_cast<CSSValueListImpl*>(property->value())->append(value);
            value->deref();
        } else if (id == CSS_PROP_SRC) {
            overriddenSrcProperty = property;
            continue;
        }

        propList->append(property);
    }

    if (overriddenSrcProperty) {
        propList->append(overriddenSrcProperty);
    }

    numParsedProperties = 0;
    return new CSSStyleDeclarationImpl(rule, propList);
}

void CSSParser::clearProperties()
{
    for ( int i = 0; i < numParsedProperties; i++ )
        delete parsedProperties[i];
    numParsedProperties = 0;
}

DOM::DocumentImpl *CSSParser::document() const
{
    if (!styleDocument) {
        const StyleBaseImpl* root = styleElement;
        while (root->parent())
            root = root->parent();
        if (root->isCSSStyleSheet())
            styleDocument = static_cast<const CSSStyleSheetImpl*>(root)->doc();
    }
    return styleDocument;
}

bool CSSParser::validUnit( Value *value, int unitflags, bool strict )
{
    if ( unitflags & FNonNeg && value->fValue < 0 )
        return false;

    bool b = false;
    switch( value->unit ) {
    case CSSPrimitiveValue::CSS_NUMBER:
        b = (unitflags & FNumber);
        if ( !b && ( (unitflags & FLength) && (value->fValue == 0 || !strict ) ) ) {
            value->unit = CSSPrimitiveValue::CSS_PX;
            b = true;
        }
        if (!b && (unitflags & FInteger) && value->isInt)
            b = true;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        b = (unitflags & FPercent);
        break;
    case Value::Q_EMS:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_CHS:
    case CSSPrimitiveValue::CSS_REMS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        b = (unitflags & FLength);
        break;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        b = (unitflags & FTime);
        break;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DPI:
    case CSSPrimitiveValue::CSS_DPCM:
    case CSSPrimitiveValue::CSS_DIMENSION:
    default:
        break;
    }
    return b;
}

bool CSSParser::parseValue( int propId, bool important )
{
    if ( !valueList ) return false;

    Value *value = valueList->current();

    if ( !value )
        return false;

    int id = value->id;

    const int num = inShorthand() ? 1 : valueList->size();

    if ( id == CSS_VAL_INHERIT ) {
        if (num != 1)
            return false;
        addProperty( propId, new CSSInheritedValueImpl(), important );
        return true;
    } else if (id == CSS_VAL_INITIAL ) {
        if (num != 1)
            return false;
        addProperty(propId, new CSSInitialValueImpl(false/*implicit initial*/), important);
        return true;
    }

    bool valid_primitive = false;
    CSSValueImpl *parsedValue = 0;

    switch(propId) {
        /* The comment to the left defines all valid value of this properties as defined
         * in CSS 2, Appendix F. Property index
         */

        /* All the CSS properties are not supported by the renderer at the moment.
         * Note that all the CSS2 Aural properties are only checked, if CSS_AURAL is defined
         * (see parseAuralValues). As we don't support them at all this seems reasonable.
         */

    case CSS_PROP_SIZE:                 // <length>{1,2} | auto | portrait | landscape | inherit
//     case CSS_PROP_PAGE:                 // <identifier> | auto // ### CHECK
        // ### To be done
        if (id)
            valid_primitive = true;
        break;
    case CSS_PROP_UNICODE_BIDI:         // normal | embed | bidi-override | inherit
        if ( id == CSS_VAL_NORMAL ||
             id == CSS_VAL_EMBED ||
             id == CSS_VAL_BIDI_OVERRIDE )
            valid_primitive = true;
        break;

    case CSS_PROP_POSITION:             // static | relative | absolute | fixed | inherit
        if ( id == CSS_VAL_STATIC ||
             id == CSS_VAL_RELATIVE ||
             id == CSS_VAL_ABSOLUTE ||
              id == CSS_VAL_FIXED )
            valid_primitive = true;
        break;

    case CSS_PROP_PAGE_BREAK_AFTER:     // auto | always | avoid | left | right | inherit
    case CSS_PROP_PAGE_BREAK_BEFORE:    // auto | always | avoid | left | right | inherit
        if ( id == CSS_VAL_AUTO ||
             id == CSS_VAL_ALWAYS ||
             id == CSS_VAL_AVOID ||
              id == CSS_VAL_LEFT ||
              id == CSS_VAL_RIGHT )
            valid_primitive = true;
        break;

    case CSS_PROP_PAGE_BREAK_INSIDE:    // avoid | auto | inherit
        if ( id == CSS_VAL_AUTO ||
             id == CSS_VAL_AVOID )
            valid_primitive = true;
        break;

    case CSS_PROP_EMPTY_CELLS:          // show | hide | inherit
        if ( id == CSS_VAL_SHOW ||
             id == CSS_VAL_HIDE )
            valid_primitive = true;
        break;

    case CSS_PROP_QUOTES:               // [<string> <string>]+ | none | inherit
        if (id == CSS_VAL_NONE) {
            valid_primitive = true;
        } else {
            QuotesValueImpl *quotes = new QuotesValueImpl;
            bool is_valid = true;
            QString open, close;
            Value *val=valueList->current();
            while (val) {
                if (val->unit == CSSPrimitiveValue::CSS_STRING)
                    open = qString(val->string);
                else {
                    is_valid = false;
                    break;
                }
                valueList->next();
                val=valueList->current();
                if (val && val->unit == CSSPrimitiveValue::CSS_STRING)
                    close = qString(val->string);
                else {
                    is_valid = false;
                    break;
                }
                quotes->addLevel(open, close);
                valueList->next();
                val=valueList->current();
            }
            if (is_valid)
                parsedValue = quotes;
            else
                delete quotes;
        }
        break;

    case CSS_PROP_CONTENT:     //  normal | none | inherit |
        // [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+
        if ( id == CSS_VAL_NORMAL || id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            return parseContent( propId, important );
        break;

    case CSS_PROP_WHITE_SPACE:          // normal | pre | nowrap | pre-wrap | pre-line | inherit
        if ( id == CSS_VAL_NORMAL ||
             id == CSS_VAL_PRE ||
             id == CSS_VAL_PRE_WRAP ||
             id == CSS_VAL_PRE_LINE ||
             id == CSS_VAL_NOWRAP )
            valid_primitive = true;
        break;

    case CSS_PROP_CLIP:                 // <shape> | auto | inherit
        if ( id == CSS_VAL_AUTO )
            valid_primitive = true;
        else if ( value->unit == Value::Function )
            return parseShape( propId, important );
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in khtml::applyRule(..)
     */
    case CSS_PROP_CAPTION_SIDE:         // top | bottom | left | right | inherit
        // Left and right were deprecated in CSS 2.1 and never supported by KHTML
        if ( /* id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT || */
            id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM)
            valid_primitive = true;
        break;

    case CSS_PROP_BORDER_COLLAPSE:      // collapse | separate | inherit
        if ( id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE )
            valid_primitive = true;
        break;

    case CSS_PROP_VISIBILITY:           // visible | hidden | collapse | inherit
        if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_COLLAPSE)
            valid_primitive = true;
        break;

    case CSS_PROP_OVERFLOW:             // visible | hidden | scroll | auto | marquee | inherit
    case CSS_PROP_OVERFLOW_X:
    case CSS_PROP_OVERFLOW_Y:
        if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_SCROLL || id == CSS_VAL_AUTO ||
            id == CSS_VAL_MARQUEE)
            valid_primitive = true;
        break;

    case CSS_PROP_LIST_STYLE_POSITION:  // inside | outside | inherit
        if ( id == CSS_VAL_INSIDE || id == CSS_VAL_OUTSIDE )
            valid_primitive = true;
        break;

    case CSS_PROP_LIST_STYLE_TYPE:
        // disc | circle | square | decimal | decimal-leading-zero | lower-roman |
        // upper-roman | lower-greek | lower-alpha | lower-latin | upper-alpha |
        // upper-latin | hebrew | armenian | georgian | cjk-ideographic | hiragana |
        // katakana | hiragana-iroha | katakana-iroha | none | inherit
        if ((id >= CSS_VAL_DISC && id <= CSS_VAL__KHTML_CLOSE_QUOTE) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP_DISPLAY:
        // inline | block | list-item | run-in | inline-block | -khtml-ruler | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | none | inherit
        if ((id >= CSS_VAL_INLINE && id <= CSS_VAL_TABLE_CAPTION) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP_DIRECTION:            // ltr | rtl | inherit
        if ( id == CSS_VAL_LTR || id == CSS_VAL_RTL )
            valid_primitive = true;
        break;

    case CSS_PROP_TEXT_TRANSFORM:       // capitalize | uppercase | lowercase | none | inherit
        if ((id >= CSS_VAL_CAPITALIZE && id <= CSS_VAL_LOWERCASE) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP_FLOAT:                // left | right | none | khtml_left | khtml_right | inherit + center for buggy CSS
        if ( id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT || id == CSS_VAL__KHTML_LEFT ||
             id == CSS_VAL__KHTML_RIGHT ||id == CSS_VAL_NONE || id == CSS_VAL_CENTER)
            valid_primitive = true;
        break;

    case CSS_PROP_CLEAR:                // none | left | right | both | inherit
        if ( id == CSS_VAL_NONE || id == CSS_VAL_LEFT ||
             id == CSS_VAL_RIGHT|| id == CSS_VAL_BOTH)
            valid_primitive = true;
        break;

    case CSS_PROP_TEXT_ALIGN:
        // left | right | center | justify | khtml_left | khtml_right | khtml_center | <string> | inherit
        if ( ( id >= CSS_VAL__KHTML_AUTO && id <= CSS_VAL__KHTML_CENTER ) ||
             value->unit == CSSPrimitiveValue::CSS_STRING )
            valid_primitive = true;
        break;

    case CSS_PROP_OUTLINE_STYLE:        // <border-style> | inherit
    case CSS_PROP_BORDER_TOP_STYLE:     //// <border-style> | inherit
    case CSS_PROP_BORDER_RIGHT_STYLE:   //   Defined as:    none | hidden | dotted | dashed |
    case CSS_PROP_BORDER_BOTTOM_STYLE:  //   solid | double | groove | ridge | inset | outset | -khtml-native
    case CSS_PROP_BORDER_LEFT_STYLE:    ////
        if (id >= CSS_VAL__KHTML_NATIVE && id <= CSS_VAL_DOUBLE)
            valid_primitive = true;
        break;

    case CSS_PROP_FONT_WEIGHT: // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | inherit
        id = parseFontWeight(value, false);
        if (id) {
            valid_primitive = true;
        }
        break;
    case CSS_PROP_BORDER_TOP_RIGHT_RADIUS:
    case CSS_PROP_BORDER_BOTTOM_RIGHT_RADIUS:
    case CSS_PROP_BORDER_BOTTOM_LEFT_RADIUS:
    case CSS_PROP_BORDER_TOP_LEFT_RADIUS: {
        //<length> <length>?
        if (num < 1 || num > 2)
            return false;

        if (!validUnit( value, FLength|FPercent|FNonNeg, strict))
            return false;

        CSSPrimitiveValueImpl* horiz = new CSSPrimitiveValueImpl( value->fValue,
                                                     (CSSPrimitiveValue::UnitTypes) value->unit );
        CSSPrimitiveValueImpl* vert;

        if (num == 2) {
            value = valueList->next();
            if (!validUnit( value, FLength|FPercent|FNonNeg, strict)) {
                delete horiz;
                return false;
            }
            vert = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
        } else {
            vert = horiz;
        }

        addProperty(propId, new CSSPrimitiveValueImpl(new PairImpl(horiz, vert)), important);
        return true;
    }

    case CSS_PROP_BORDER_RADIUS:
        return parseBorderRadius(important);

    case CSS_PROP_BORDER_SPACING:
    {
        const int properties[2] = { CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING,
                                    CSS_PROP__KHTML_BORDER_VERTICAL_SPACING };
        if (num == 1) {
            ShorthandScope scope(this, CSS_PROP_BORDER_SPACING);
            if (!parseValue(properties[0], important)) return false;
            CSSValueImpl* value = parsedProperties[numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSS_PROP_BORDER_SPACING);
            const int oldNumParsedProperties = numParsedProperties;
            if (!parseValue(properties[0], important)) return false;
            if (!parseValue(properties[1], important)) {
                rollbackParsedProperties(oldNumParsedProperties);
                return false;
            }
            return true;
        }
        return false;
    }
    case CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING:
    case CSS_PROP__KHTML_BORDER_VERTICAL_SPACING:
        valid_primitive = validUnit(value, FLength|FNonNeg, strict);
        break;

    case CSS_PROP_SCROLLBAR_FACE_COLOR:         // IE5.5
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:       // IE5.5
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:    // IE5.5
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:      // IE5.5
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:   // IE5.5
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:        // IE5.5
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:        // IE5.5
    case CSS_PROP_SCROLLBAR_BASE_COLOR:         // IE5.5
        if ( strict )
            break;
        [[fallthrough]];
    case CSS_PROP_OUTLINE_COLOR:        // <color> | invert | inherit
        // outline has "invert" as additional keyword.
        if ( propId == CSS_PROP_OUTLINE_COLOR && id == CSS_VAL_INVERT ) {
            valid_primitive = true;
            break;
        }
        [[fallthrough]];
    case CSS_PROP_BACKGROUND_COLOR:     // <color> | inherit
    case CSS_PROP_BORDER_TOP_COLOR:     // <color> | inherit
    case CSS_PROP_BORDER_RIGHT_COLOR:   // <color> | inherit
    case CSS_PROP_BORDER_BOTTOM_COLOR:  // <color> | inherit
    case CSS_PROP_BORDER_LEFT_COLOR:    // <color> | inherit
    case CSS_PROP_COLOR:                // <color> | inherit
        if ( id == CSS_VAL__KHTML_TEXT || id == CSS_VAL_MENU ||
             (id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT ) ||
             id == CSS_VAL_TRANSPARENT ||
             id == CSS_VAL_CURRENTCOLOR ||
             (id >= CSS_VAL_GREY && id < CSS_VAL__KHTML_TEXT && !strict ) ) {
            valid_primitive = true;
        } else {
            parsedValue = parseColor();
            if ( parsedValue )
                valueList->next();
        }
        break;

    case CSS_PROP_CURSOR:
        //  [ auto | default | none |
	//  context-menu | help | pointer | progress | wait |
	//  cell | crosshair | text | vertical-text |
	//  alias | copy | move | no-drop | not-allowed |
	//  e-resize | ne-resize | nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize |
	//  ew-resize | ns-resize | nesw-resize | nwse-resize |
	//  col-resize | row-resize | all-scroll
        // ] ] | inherit
    // MSIE 5 compatibility :/
        if ( !strict && id == CSS_VAL_HAND ) {
            id = CSS_VAL_POINTER;
            valid_primitive = true;
        } else if (( id >= CSS_VAL_AUTO && id <= CSS_VAL_ALL_SCROLL ) || id == CSS_VAL_NONE )
            valid_primitive = true;
        break;

    case CSS_PROP_BACKGROUND_ATTACHMENT:
    case CSS_PROP_BACKGROUND_CLIP:
    case CSS_PROP_BACKGROUND_IMAGE:
    case CSS_PROP_BACKGROUND_ORIGIN:
    case CSS_PROP_BACKGROUND_POSITION:
    case CSS_PROP_BACKGROUND_POSITION_X:
    case CSS_PROP_BACKGROUND_POSITION_Y:
    case CSS_PROP_BACKGROUND_SIZE:
    case CSS_PROP_BACKGROUND_REPEAT: {
        CSSValueImpl *val1 = 0, *val2 = 0;
        int propId1, propId2;
        if (parseBackgroundProperty(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1, important);
            if (val2)
                addProperty(propId2, val2, important);
            return true;
        }
        return false;
    }
    case CSS_PROP_LIST_STYLE_IMAGE:     // <uri> | none | inherit
        if (id == CSS_VAL_NONE) {
            parsedValue = new CSSImageValueImpl();
            valueList->next();
        }
        else if (value->unit == CSSPrimitiveValue::CSS_URI ) {
            // ### allow string in non strict mode?
            if (styleElement) {
                parsedValue = new CSSImageValueImpl(domString(value->string), styleElement);
                valueList->next();
            }
        }
        break;

    case CSS_PROP_OUTLINE_WIDTH:        // <border-width> | inherit
    case CSS_PROP_BORDER_TOP_WIDTH:     //// <border-width> | inherit
    case CSS_PROP_BORDER_RIGHT_WIDTH:   //   Which is defined as
    case CSS_PROP_BORDER_BOTTOM_WIDTH:  //   thin | medium | thick | <length>
    case CSS_PROP_BORDER_LEFT_WIDTH:    ////
        if (id == CSS_VAL_THIN || id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK)
            valid_primitive = true;
        else
            valid_primitive = ( validUnit( value, FLength|FNonNeg, strict ) );
        break;

    case CSS_PROP_LETTER_SPACING:       // normal | <length> | inherit
    case CSS_PROP_WORD_SPACING:         // normal | <length> | inherit
        if ( id == CSS_VAL_NORMAL )
            valid_primitive = true;
        else
            valid_primitive = validUnit( value, FLength, strict );
        break;

    case CSS_PROP_TEXT_INDENT:          //  <length> | <percentage> | inherit
        valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict ) );
        break;

    case CSS_PROP_PADDING_TOP:          //  <length> | <percentage> | inherit
    case CSS_PROP_PADDING_RIGHT:        //  <padding-width> | inherit
    case CSS_PROP_PADDING_BOTTOM:       //   Which is defined as
    case CSS_PROP_PADDING_LEFT:         //   <length> | <percentage>
    case CSS_PROP__KHTML_PADDING_START:
        valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict ) );
        break;

    case CSS_PROP_MAX_HEIGHT:           // <length> | <percentage> | none | inherit
    case CSS_PROP_MAX_WIDTH:            // <length> | <percentage> | none | inherit
        if ( id == CSS_VAL_NONE ) {
            valid_primitive = true;
            break;
        }
        [[fallthrough]];
    case CSS_PROP_MIN_HEIGHT:           // <length> | <percentage> | inherit
    case CSS_PROP_MIN_WIDTH:            // <length> | <percentage> | inherit
            valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict ) );
        break;

    case CSS_PROP_FONT_SIZE:
            // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
        if (id >= CSS_VAL_XX_SMALL && id <= CSS_VAL_LARGER)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent|FNonNeg, strict);
        break;

    case CSS_PROP_FONT_STYLE:           // normal | italic | oblique | inherit
        if ( id == CSS_VAL_NORMAL || id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE)
            valid_primitive = true;
        break;

    case CSS_PROP_FONT_VARIANT:         // normal | small-caps | inherit
        if ( id == CSS_VAL_NORMAL || id == CSS_VAL_SMALL_CAPS)
            valid_primitive = true;
        break;

    case CSS_PROP_VERTICAL_ALIGN:
            // baseline | sub | super | top | text-top | middle | bottom | text-bottom |
        // <percentage> | <length> | inherit

        if ( id >= CSS_VAL_BASELINE && id <= CSS_VAL__KHTML_BASELINE_MIDDLE )
            valid_primitive = true;
        else
            valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict ) );
        break;

    case CSS_PROP_HEIGHT:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_WIDTH:                // <length> | <percentage> | auto | inherit
        if ( id == CSS_VAL_AUTO )
            valid_primitive = true;
        else
            // ### handle multilength case where we allow relative units
            valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict ) );
        break;

    case CSS_PROP_BOTTOM:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_LEFT:                 // <length> | <percentage> | auto | inherit
    case CSS_PROP_RIGHT:                // <length> | <percentage> | auto | inherit
    case CSS_PROP_TOP:                  // <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_TOP:           //// <margin-width> | inherit
    case CSS_PROP_MARGIN_RIGHT:         //   Which is defined as
    case CSS_PROP_MARGIN_BOTTOM:        //   <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_LEFT:          ////
    case CSS_PROP__KHTML_MARGIN_START:
        if ( id == CSS_VAL_AUTO )
            valid_primitive = true;
        else
            valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict ) );
        break;

    case CSS_PROP_Z_INDEX:              // auto | <integer> | inherit
        // qDebug("parsing z-index: id=%d, fValue=%f", id, value->fValue );
        if ( id == CSS_VAL_AUTO ) {
            valid_primitive = true;
            break;
        }
        [[fallthrough]];
    case CSS_PROP_ORPHANS:              // <integer> | inherit
    case CSS_PROP_WIDOWS:               // <integer> | inherit
        // ### not supported later on
        valid_primitive = ( !id && validUnit( value, FInteger, false ) );
        break;

    case CSS_PROP_LINE_HEIGHT:          // normal | <number> | <length> | <percentage> | inherit
        if ( id == CSS_VAL_NORMAL )
            valid_primitive = true;
        else
            valid_primitive = ( !id && validUnit( value, FNumber|FLength|FPercent|FNonNeg, strict ) );
        break;
    case CSS_PROP_COUNTER_INCREMENT:    // [ <identifier> <integer>? ]+ | none | inherit
        if ( id == CSS_VAL_NONE )
            valid_primitive = true;
        else
            return parseCounter(propId, true, important);
        break;
    case CSS_PROP_COUNTER_RESET:        // [ <identifier> <integer>? ]+ | none | inherit
        if ( id == CSS_VAL_NONE )
            valid_primitive = true;
        else
            return parseCounter(propId, false, important);
            break;

    case CSS_PROP_FONT_FAMILY:
            // [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
        parsedValue = parseFontFamily();
        break;
    }

    case CSS_PROP_TEXT_DECORATION:
            // none | [ underline || overline || line-through || blink ] | inherit
        if (id == CSS_VAL_NONE) {
            valid_primitive = true;
        } else {
            CSSValueListImpl *list = new CSSValueListImpl;
            bool is_valid = true;
            while( is_valid && value ) {
                switch ( value->id ) {
                case CSS_VAL_BLINK:
                    break;
                case CSS_VAL_UNDERLINE:
                case CSS_VAL_OVERLINE:
                case CSS_VAL_LINE_THROUGH:
                    list->append( new CSSPrimitiveValueImpl( value->id ) );
                    break;
                default:
                    is_valid = false;
                }
                value = valueList->next();
            }
            //kDebug( 6080 ) << "got " << list->length() << "d decorations";
            if(list->length() && is_valid) {
                parsedValue = list;
                valueList->next();
            } else {
                delete list;
            }
        }
        break;

    case CSS_PROP_TABLE_LAYOUT:         // auto | fixed | inherit
        if ( id == CSS_VAL_AUTO || id == CSS_VAL_FIXED )
            valid_primitive = true;
        break;

    case CSS_PROP_SRC:  // Only used within @font-face, so cannot use inherit | initial or be !important.  This is a list of urls or local references.
        return parseFontFaceSrc();

    case CSS_PROP_UNICODE_RANGE:
        // return parseFontFaceUnicodeRange();
        break;

    case CSS_PROP__KHTML_FLOW_MODE:
        if ( id == CSS_VAL__KHTML_NORMAL || id == CSS_VAL__KHTML_AROUND_FLOATS )
            valid_primitive = true;
        break;

    /* CSS3 properties */
    case CSS_PROP_BOX_SIZING:        // border-box | content-box | inherit
        if ( id == CSS_VAL_BORDER_BOX || id == CSS_VAL_CONTENT_BOX )
            valid_primitive = true;
        break;
    case CSS_PROP_OUTLINE_OFFSET:
        valid_primitive = validUnit(value, FLength, strict);
        break;
    case CSS_PROP_TEXT_SHADOW:  // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            return parseShadow(propId, important);
        break;
    case CSS_PROP_OPACITY:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__KHTML_USER_INPUT:        // none | enabled | disabled | inherit
        if ( id == CSS_VAL_NONE || id == CSS_VAL_ENABLED || id == CSS_VAL_DISABLED )
            valid_primitive = true;
//        kDebug(6080) << "CSS_PROP__KHTML_USER_INPUT: " << valid_primitive;
        break;
    case CSS_PROP__KHTML_MARQUEE: {
        const int properties[5] = { CSS_PROP__KHTML_MARQUEE_DIRECTION, CSS_PROP__KHTML_MARQUEE_INCREMENT,
                                    CSS_PROP__KHTML_MARQUEE_REPETITION,
                                    CSS_PROP__KHTML_MARQUEE_STYLE, CSS_PROP__KHTML_MARQUEE_SPEED };
        return parseShortHand(propId, properties, 5, important);
    }
    case CSS_PROP__KHTML_MARQUEE_DIRECTION:
        if (id == CSS_VAL_FORWARDS || id == CSS_VAL_BACKWARDS || id == CSS_VAL_AHEAD ||
            id == CSS_VAL_REVERSE || id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT || id == CSS_VAL_DOWN ||
            id == CSS_VAL_UP || id == CSS_VAL_AUTO)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_MARQUEE_INCREMENT:
        if (id == CSS_VAL_SMALL || id == CSS_VAL_LARGE || id == CSS_VAL_MEDIUM)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent, strict);
        break;
    case CSS_PROP__KHTML_MARQUEE_STYLE:
        if (id == CSS_VAL_NONE || id == CSS_VAL_SLIDE || id == CSS_VAL_SCROLL || id == CSS_VAL_ALTERNATE ||
            id == CSS_VAL_UNFURL)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_MARQUEE_REPETITION:
        if (id == CSS_VAL_INFINITE)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FInteger|FNonNeg, strict);
        break;
    case CSS_PROP__KHTML_MARQUEE_SPEED:
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_SLOW || id == CSS_VAL_FAST)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FTime|FInteger|FNonNeg, strict);
        break;
    case CSS_PROP_TEXT_OVERFLOW: // clip | ellipsis
        if (id == CSS_VAL_CLIP || id == CSS_VAL_ELLIPSIS)
            valid_primitive = true;
        break;
    // End of CSS3 properties

        /* shorthand properties */
    case CSS_PROP_BACKGROUND:
            // ['background-color' || 'background-image' ||'background-repeat' ||
        // 'background-attachment' || 'background-position'] | inherit
	return parseBackgroundShorthand(important);
    case CSS_PROP_BORDER:
         // [ 'border-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
                                    CSS_PROP_BORDER_COLOR };
        return parseShortHand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_TOP:
            // [ 'border-top-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
                                    CSS_PROP_BORDER_TOP_COLOR};
        return parseShortHand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_RIGHT:
            // [ 'border-right-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
                                    CSS_PROP_BORDER_RIGHT_COLOR };
        return parseShortHand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_BOTTOM:
            // [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
                                    CSS_PROP_BORDER_BOTTOM_COLOR };
        return parseShortHand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_LEFT:
            // [ 'border-left-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
                                    CSS_PROP_BORDER_LEFT_COLOR };
        return parseShortHand(propId, properties, 3, important);
    }
    case CSS_PROP_OUTLINE:
            // [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
    {
        const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
                                    CSS_PROP_OUTLINE_COLOR };
        return parseShortHand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_COLOR:
            // <color>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
                                    CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_BORDER_WIDTH:
            // <border-width>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
                                    CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_BORDER_STYLE:
            // <border-style>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
                                    CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_MARGIN:
            // <margin-width>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
                                    CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_PADDING:
            // <padding-width>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
                                    CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_FONT:
    {
        return parseFontShorthand(important);
    }
    case CSS_PROP_LIST_STYLE:
    {
        return parseListStyleShorthand(important);
    }
    case CSS_PROP_WORD_WRAP:
        // normal | break-word
        if ( id == CSS_VAL_NORMAL || id == CSS_VAL_BREAK_WORD )
            valid_primitive = true;
        break;

    default:
        return parseSVGValue(propId, important);
// #ifdef CSS_DEBUG
//         kDebug( 6080 ) << "illegal or CSS2 Aural property: " << val;
// #endif
        //break;
    }

    if ( valid_primitive ) {

        if ( id != 0 ) {
            parsedValue = new CSSPrimitiveValueImpl( id );
        } else if ( value->unit == CSSPrimitiveValue::CSS_STRING )
            parsedValue = new CSSPrimitiveValueImpl( domString( value->string ),
                                                     (CSSPrimitiveValue::UnitTypes) value->unit );
        else if ( value->unit >= CSSPrimitiveValue::CSS_NUMBER &&
                  value->unit <= CSSPrimitiveValue::CSS_KHZ ) {
            parsedValue = new CSSPrimitiveValueImpl( value->fValue,
                                                     (CSSPrimitiveValue::UnitTypes) value->unit );
        } else if ( value->unit >= Value::Q_EMS ) {
            parsedValue = new CSSQuirkPrimitiveValueImpl( value->fValue, CSSPrimitiveValue::CSS_EMS );
        }
        valueList->next();
    }
    if ( parsedValue ) {
        if (!valueList->current() || inShorthand()) {
            addProperty( propId, parsedValue, important );
            return true;
        }
        delete parsedValue;
    }
    return false;
}

void CSSParser::addBackgroundValue(CSSValueImpl*& lval, CSSValueImpl* rval)
{
    if (lval) {
        if (lval->isValueList())
            static_cast<CSSValueListImpl*>(lval)->append(rval);
        else {
            CSSValueImpl* oldVal = lval;
            CSSValueListImpl* list = new CSSValueListImpl(CSSValueListImpl::Comma);
            lval = list;
            list->append(oldVal);
            list->append(rval);
        }
    }
    else
        lval = rval;
}

bool CSSParser::parseBackgroundShorthand(bool important)
{
    // Order is important in this array:
    // 'position' must come before color because a plain old "0" is a legal color in quirks mode
    //  but it's usually the X coordinate of a position.
    // 'size' must be the next property after 'position' in order to correctly parse '/size'.
    // 'origin' must come before 'clip' because the first <box> value found belongs to 'origin',
    //  the second (if any) to 'clip'.
    const int numProperties = 8;
    const int properties[numProperties] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT,
        CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION, CSS_PROP_BACKGROUND_SIZE,
        CSS_PROP_BACKGROUND_ORIGIN, CSS_PROP_BACKGROUND_CLIP, CSS_PROP_BACKGROUND_COLOR };

    ShorthandScope scope(this, CSS_PROP_BACKGROUND);

    bool parsedProperty[numProperties] = { false }; // compiler will repeat false as necessary
    CSSValueImpl* values[numProperties] = { 0 }; // compiler will repeat 0 as necessary
    CSSValueImpl* positionYValue = 0;
    int parsedOriginIdent = 0;
    int i;

    while (valueList->current()) {
        Value* val = valueList->current();
        if (val->unit == Value::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSS_PROP_BACKGROUND_COLOR && parsedProperty[i])
                    // Color is not allowed except as the last item in a list.  Reject the entire
                    // property.
                    goto fail;

                if (!parsedProperty[i] && properties[i] != CSS_PROP_BACKGROUND_COLOR) {
                    if (properties[i] == CSS_PROP_BACKGROUND_CLIP && parsedOriginIdent != 0) {
                        addBackgroundValue(values[i], new CSSPrimitiveValueImpl(parsedOriginIdent));
                    } else {
                        addBackgroundValue(values[i], new CSSInitialValueImpl(true/*implicit initial*/));
                        if (properties[i] == CSS_PROP_BACKGROUND_POSITION) {
                            addBackgroundValue(positionYValue, new CSSInitialValueImpl(true/*implicit initial*/));
                        }
                    }
                }
                parsedProperty[i] = false;
            }
            parsedOriginIdent = 0;
            if (!valueList->current())
                break;
        }

        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                CSSValueImpl *val1 = 0, *val2 = 0;
                int propId1, propId2;
                if (parseBackgroundProperty(properties[i], propId1, propId2, val1, val2)) {
                    parsedProperty[i] = found = true;
                    addBackgroundValue(values[i], val1);
                    if (properties[i] == CSS_PROP_BACKGROUND_POSITION) {
                        addBackgroundValue(positionYValue, val2);
                        // after 'position' there could be '/size', check for it
                        const Value* v = valueList->current();
                        if (v && v->unit == Value::Operator && v->iValue == '/') {
                            // next property _must_ be 'size'
                            valueList->next();
                            ++i; // 'size' is at the next position in properties[] array
                            CSSValueImpl *retVal1 = 0, *retVal2 = 0;
                            if (parseBackgroundProperty(properties[i], propId1, propId2, retVal1, retVal2)) {
                                parsedProperty[i] = true;
                                addBackgroundValue(values[i], retVal1);
                            } else {
                                goto fail;
                            }
                        }
                    } else if (properties[i] == CSS_PROP_BACKGROUND_ORIGIN) {
                        parsedOriginIdent = static_cast<CSSPrimitiveValueImpl*>(val1)->getIdent();
                    } else if (properties[i] == CSS_PROP_BACKGROUND_SIZE) {
                        // we caught an invalid length|percent as background-size
                        goto fail;
                    }
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            goto fail;

    } // end of while loop

    // Fill in any remaining properties with the initial value.
    for (i = 0; i < numProperties; ++i) {
        if (!parsedProperty[i]) {
            if (properties[i] == CSS_PROP_BACKGROUND_CLIP && parsedOriginIdent != 0) {
                addBackgroundValue(values[i], new CSSPrimitiveValueImpl(parsedOriginIdent));
            } else {
                addBackgroundValue(values[i], new CSSInitialValueImpl(true/*implicit initial*/));
                if (properties[i] == CSS_PROP_BACKGROUND_POSITION) {
                    addBackgroundValue(positionYValue, new CSSInitialValueImpl(true/*implicit initial*/));
                }
            }
        }
    }

    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++) {
        if (properties[i] == CSS_PROP_BACKGROUND_POSITION) {
            addProperty(CSS_PROP_BACKGROUND_POSITION_X, values[i], important);
            addProperty(CSS_PROP_BACKGROUND_POSITION_Y, positionYValue, important);
        }
        else
            addProperty(properties[i], values[i], important);
    }

    return true;

fail:
    for (int k = 0; k < numProperties; k++)
        delete values[k];
    delete positionYValue;
    return false;
}

static void completeMissingRadii(SharedPtr<CSSPrimitiveValueImpl>* array)
{
    if (!array[1])
        array[1] = array[0]; // top-left => top-right
    if (!array[2])
        array[2] = array[0]; // top-left => bottom-right
    if (!array[3])
        array[3] = array[1]; // top-left => bottom-right
}

bool CSSParser::parseBorderRadius(bool important) {
    const int properties[4] = { CSS_PROP_BORDER_TOP_LEFT_RADIUS,
                                CSS_PROP_BORDER_TOP_RIGHT_RADIUS,
                                CSS_PROP_BORDER_BOTTOM_RIGHT_RADIUS,
                                CSS_PROP_BORDER_BOTTOM_LEFT_RADIUS };
    SharedPtr<CSSPrimitiveValueImpl> horiz[4], vert[4];


    for (int c = 0; c < 4; ++c) {
        horiz[c] = 0;
        vert [c] = 0;
    }

    Value* value;

    // Parse horizontal ones until / or done.
    for (int c = 0; c < 4; ++c) {
        value = valueList->current();
        if (!value || (value->unit == Value::Operator && value->iValue == '/'))
            break; //Saw slash -- exit w/o consuming as we'll use it below.

        if (!validUnit(value, FLength|FPercent|FNonNeg, strict))
            return false;

        horiz[c] = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
        value = valueList->next();
    }

    if (!horiz[0])
        return false;

    completeMissingRadii(horiz);

    // Do we have vertical radii afterwards?
    if (value && value->unit == Value::Operator && value->iValue == '/') {
        valueList->next();

        for (int c = 0; c < 4; ++c) {
            kDebug() << c;
            value = valueList->current();
            if (!value)
                break;

            if (!validUnit(value, FLength|FPercent|FNonNeg, strict))
                return false;

            vert[c] = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
            value = valueList->next();
        }

        // If we didn't parse anything, or there is stuff remaining, this is malformed
        if (!vert[0] || valueList->current())
            return false;

        completeMissingRadii(vert);
    } else {
        // Nope -- we better be at the end.
        if (valueList->current())
            return false;

        for (int c = 0; c < 4; ++c)
            vert[c] = horiz[c];
    }

    // All OK parsing, add properties
    for (int c = 0; c < 4; ++c)
        addProperty(properties[c], new CSSPrimitiveValueImpl(
                                        new PairImpl(horiz[c].get(), vert[c].get())), important);
    return true;
}

bool CSSParser::parseShortHand(int propId, const int *properties, const int numProperties, bool important)
{
    if (valueList->size() > numProperties) {
        // discard
        return false;
    }

    ShorthandScope scope(this, propId);

    // Store current numParsedProperties, we need it in case we should rollback later
    const int oldNumParsedProperties = numParsedProperties;

    // Setup an array of booleans to mark which property has been found
    bool fnd[6]; //Trust me ;)
    for( int i = 0; i < numProperties; i++ )
        fnd[i] = false;

    bool discard = false;
    unsigned short numValidProperties = 0;
    bool foundValid = false;
    while ( valueList->current() ) {
        foundValid = false;
        for (int propIndex = 0; propIndex < numProperties; ++propIndex) {
            if (parseValue(properties[propIndex], important)) {
                foundValid = true;
                ++numValidProperties;
                if (fnd[propIndex]) { // found a duplicate
                    discard = true;
                } else {
                    fnd[propIndex] = true;
                }

                break;
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!foundValid) {
            discard = true;
        }

        if (discard) {
            break;
        }
    }

    if (discard) {
        // Remove valid properties previously added by parseValue(), if any
        rollbackParsedProperties(oldNumParsedProperties);
        return false;
    }

    if (numValidProperties == numProperties) {
        return true;
    }

    // Fill in any remaining properties with the initial value.
    m_implicitShorthand = true;
    for (int i = 0; i < numProperties; ++i) {
        if (!fnd[i])
            addProperty(properties[i], new CSSInitialValueImpl(true/*implicit initial*/), important);
    }
    m_implicitShorthand = false;

    return true;
}

bool CSSParser::parse4Values(int propId, const int *properties,  bool important )
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */

    const int num = inShorthand() ? 1 : valueList->size();
    //qDebug("parse4Values: num=%d %d", num,  valueList->numValues );

    ShorthandScope scope(this, propId);

    const int oldNumParsedProperties = numParsedProperties;
    // the order is top, right, bottom, left
    switch (num) {
        case 1: {
            if (!parseValue(properties[0], important))
                return false;
            CSSValueImpl *value = parsedProperties[numParsedProperties-1]->value();
            m_implicitShorthand = true;
            addProperty(properties[1], value, important);
            addProperty(properties[2], value, important);
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
            break;
        }
        case 2: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important)) {
                rollbackParsedProperties(oldNumParsedProperties);
                return false;
                }
            CSSValueImpl *value = parsedProperties[numParsedProperties-2]->value();
            m_implicitShorthand = true;
            addProperty(properties[2], value, important);
            value = parsedProperties[numParsedProperties-2]->value();
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
            break;
        }
        case 3: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) || !parseValue(properties[2], important)) {
                rollbackParsedProperties(oldNumParsedProperties);
                return false;
            }
            CSSValueImpl *value = parsedProperties[numParsedProperties-2]->value();
            m_implicitShorthand = true;
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
            break;
        }
        case 4: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) ||
                !parseValue(properties[2], important) || !parseValue(properties[3], important)) {
                rollbackParsedProperties(oldNumParsedProperties);
                return false;
            }
            break;
        }
        default: {
            return false;
        }
    }

    return true;
}

// [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
// in CSS 2.1 this got somewhat reduced:
// [ <string> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
bool CSSParser::parseContent( int propId, bool important )
{
    QScopedPointer<CSSValueListImpl> values(
            new CSSValueListImpl(CSSValueListImpl::Comma));

    bool isValid = true;
    Value *val;
    CSSValueImpl *parsedValue = 0;
    while ( (val = valueList->current()) ) {
        parsedValue = 0;
        if (val->unit == CSSPrimitiveValue::CSS_URI) {
            if (styleElement) {
                parsedValue = new CSSImageValueImpl(domString(val->string), styleElement);
            }
#ifdef CSS_DEBUG
            kDebug( 6080 ) << "content, url=" << value.string() << " base=" << styleElement->baseURL().url( );
#endif
        } else if ( val->unit == Value::Function ) {
            // attr( X ) | counter( X [,Y] ) | counters( X, Y, [,Z] )
            ValueList *args = val->function->args;
            QString fname = qString( val->function->name ).toLower();
            if (!args) return false;
            if (fname == "attr(") {
                if ( args->size() != 1)
                    return false;
                Value *a = args->current();
                if (a->unit != CSSPrimitiveValue::CSS_IDENT) {
                    isValid=false;
                    break;
                }
                if (qString(a->string)[0] == '-') {
                    isValid=false;
                    break;
                }
                parsedValue = new CSSPrimitiveValueImpl(domString(a->string), CSSPrimitiveValue::CSS_ATTR);
            }
            else
            if (fname == "counter(") {
                parsedValue = parseCounterContent(args, false);
                if (!parsedValue) return false;
            } else
            if (fname == "counters(") {
                parsedValue = parseCounterContent(args, true);
                if (!parsedValue) return false;
            }
            else
                return false;

        } else if ( val->unit == CSSPrimitiveValue::CSS_IDENT ) {
            // open-quote | close-quote | no-open-quote | no-close-quote
            if ( val->id == CSS_VAL_OPEN_QUOTE ||
                 val->id == CSS_VAL_CLOSE_QUOTE ||
                 val->id == CSS_VAL_NO_OPEN_QUOTE ||
                 val->id == CSS_VAL_NO_CLOSE_QUOTE ) {
                parsedValue = new CSSPrimitiveValueImpl(val->id);
            }
        } else if ( val->unit == CSSPrimitiveValue::CSS_STRING ) {
            parsedValue = new CSSPrimitiveValueImpl(domString(val->string), CSSPrimitiveValue::CSS_STRING);
        }

        if (parsedValue)
            values->append(parsedValue);
        else {
            isValid = false;
            break;
        }
        valueList->next();
    }
    if ( isValid && values->length() ) {
        addProperty( propId, values.take(), important );
        valueList->next();
        return true;
    }

    return false;
}

CSSValueImpl* CSSParser::parseCounterContent(ValueList *args, bool counters)
{
    const int argsSize = args->size();
    if (counters || (argsSize != 1 && argsSize != 3))
        if (!counters || (argsSize != 3 && argsSize != 5))
            return 0;

    CounterImpl *counter = new CounterImpl;
    Value *i = args->current();
    if (i->unit != CSSPrimitiveValue::CSS_IDENT) goto invalid;
    if (qString(i->string)[0] == '-') goto invalid;
    counter->m_identifier = domString(i->string);
    if (counters) {
        i = args->next();
        if (i->unit != Value::Operator || i->iValue != ',') goto invalid;
        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_STRING) goto invalid;
        counter->m_separator = domString(i->string);
    }
    counter->m_listStyle = CSS_VAL_DECIMAL - CSS_VAL_DISC;
    i = args->next();
    if (i) {
        if (i->unit != Value::Operator || i->iValue != ',') goto invalid;
        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_IDENT) goto invalid;
        if (i->id < CSS_VAL_DISC || i->id > CSS_VAL__KHTML_CLOSE_QUOTE) goto invalid;
        counter->m_listStyle = i->id - CSS_VAL_DISC;
    }
    return new CSSPrimitiveValueImpl(counter);
invalid:
    delete counter;
    return 0;
}

CSSValueImpl* CSSParser::parseBackgroundColor()
{
    int id = valueList->current()->id;
    if (id == CSS_VAL__KHTML_TEXT || id == CSS_VAL_TRANSPARENT || id == CSS_VAL_CURRENTCOLOR ||
        (id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) || id == CSS_VAL_MENU ||
        (id >= CSS_VAL_GREY && id < CSS_VAL__KHTML_TEXT && !strict))
       return new CSSPrimitiveValueImpl(id);
    return parseColor();
}

CSSValueImpl* CSSParser::parseBackgroundImage(bool& didParse)
{
    Value *v = valueList->current();
    if (v->id == CSS_VAL_NONE) {
        didParse = true;
        return new CSSImageValueImpl();
    } else if (v->unit == CSSPrimitiveValue::CSS_URI) {
        didParse = true;
        if (styleElement)
            return new CSSImageValueImpl(domString(v->string), styleElement);
        else
            return 0;
    } else {
        didParse = false;
        return 0;
    }
}

CSSValueImpl* CSSParser::parseBackgroundPositionXY(BackgroundPosKind& kindOut)
{
    int id = valueList->current()->id;
    if (id == CSS_VAL_LEFT || id == CSS_VAL_TOP || id == CSS_VAL_RIGHT || id == CSS_VAL_BOTTOM || id == CSS_VAL_CENTER) {
        int percent = 0;
        if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT) {
            kindOut = BgPos_X;
            if (id == CSS_VAL_RIGHT)
                percent = 100;
        }
        else if (id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM) {
            kindOut = BgPos_Y;
            if (id == CSS_VAL_BOTTOM)
                percent = 100;
        }
        else if (id == CSS_VAL_CENTER) {
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            kindOut = BgPos_Center;
            percent = 50;
        }
        return new CSSPrimitiveValueImpl(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }

    if (validUnit(valueList->current(), FPercent|FLength, strict)) {
        kindOut = BgPos_NonKW;
        return new CSSPrimitiveValueImpl(valueList->current()->fValue,
                                         (CSSPrimitiveValue::UnitTypes)valueList->current()->unit);
    }

    return 0;
}

void CSSParser::parseBackgroundPosition(CSSValueImpl*& value1, CSSValueImpl*& value2)
{
    value1 = value2 = 0;

    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    BackgroundPosKind value1pos;
    value1 = parseBackgroundPositionXY(value1pos);
    if (!value1)
        return;

    // Parse the second value, if any.
    Value *value = valueList->next();

    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (value && value->unit == Value::Operator && value->iValue == ',')
        value = 0;

    bool secondValueSpecifiedAndValid = false;
    BackgroundPosKind value2pos = BgPos_Center; // true if not specified.
    if (value) {
        value2 = parseBackgroundPositionXY(value2pos);
        if (value2) {
            secondValueSpecifiedAndValid = true;
        } else {
            if (!inShorthand()) {
                delete value1;
                value1 = 0;
                return;
            }
        }
    }

    if (!value2)
        // Only one value was specified.  The other direction is always 50%.
        // If the one given was not a keyword, it should be viewed as 'x',
        // and so setting value2 would set y, as desired.
        // If the one value was a keyword, the swap below would put things in order
        // if needed.
        value2 = new CSSPrimitiveValueImpl(50, CSSPrimitiveValue::CSS_PERCENTAGE);

    // Check for various failures
    bool ok = true;

    // Two keywords on the same axis.
    if (value1pos == BgPos_X && value2pos == BgPos_X)
        ok = false;
    if (value1pos == BgPos_Y && value2pos == BgPos_Y)
        ok = false;

    // Will we need to swap them?
    bool swap = (value1pos == BgPos_Y || value2pos == BgPos_X);

    // If we had a non-KW value and a keyword value that's in the "wrong" position,
    // this is malformed (#169612)
    if (swap && (value1pos == BgPos_NonKW || value2pos == BgPos_NonKW))
        ok = false;

    if (!ok) {
        delete value1;
        delete value2;
        value1 = 0;
        value2 = 0;
        return;
    }

    if (swap) {
        // Swap our two values.
        CSSValueImpl* val = value2;
        value2 = value1;
        value1 = val;
    }

    if (secondValueSpecifiedAndValid) {
        valueList->next();
    }
}

CSSValueImpl* CSSParser::parseBackgroundSize()
{
    Value* value = valueList->current();

    // Parse the first value.
    CSSPrimitiveValueImpl* parsedValue1;

    if (value->id == CSS_VAL_COVER || value->id == CSS_VAL_CONTAIN) {
        valueList->next();
        return new CSSPrimitiveValueImpl(value->id);
    }

    if (value->id == CSS_VAL_AUTO)
        parsedValue1 = new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
    else if (validUnit(value, FLength|FPercent|FNonNeg, strict)) {
        parsedValue1 = new CSSPrimitiveValueImpl(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    } else {
        return 0;
    }

    // Parse the second value, if any.
    value = valueList->next();

    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (value && value->unit == Value::Operator && value->iValue == ',') {
        value = 0;
    }

    CSSPrimitiveValueImpl* parsedValue2 = 0;
    if (value) {
        if (value->id == CSS_VAL_AUTO) {
            parsedValue2 = new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
        } else if (validUnit(value, FLength|FPercent|FNonNeg, strict)) {
            parsedValue2 = new CSSPrimitiveValueImpl(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        } else if (!inShorthand()) {
            delete parsedValue1;
            return 0;
        }
    }

    if (parsedValue2) {
        valueList->next();
    } else {
        // If only one value is given the second is assumed to be ‘auto’
        parsedValue2 = new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
    }

    PairImpl* pair = new PairImpl(parsedValue1, parsedValue2);
    return new CSSPrimitiveValueImpl(pair);
}

bool CSSParser::parseBackgroundProperty(int propId, int& propId1, int& propId2,
                                        CSSValueImpl*& retValue1, CSSValueImpl*& retValue2)
{
#ifdef CSS_DEBUG
    kDebug(6080) << "parseBackgroundProperty()";
    kDebug(6080) << "LOOKING FOR: " << getPropertyName(propId).string();
#endif
    Value* val;
    CSSValueListImpl *value  = new CSSValueListImpl(CSSValueListImpl::Comma);
    CSSValueListImpl *value2 = new CSSValueListImpl(CSSValueListImpl::Comma);
    bool expectComma = false;

    retValue1 = retValue2 = 0;
    propId1 = propId;
    propId2 = propId;
    if (propId == CSS_PROP_BACKGROUND_POSITION) {
        propId1 = CSS_PROP_BACKGROUND_POSITION_X;
        propId2 = CSS_PROP_BACKGROUND_POSITION_Y;
    }

    while ((val = valueList->current())) {
        CSSValueImpl *currValue = 0, *currValue2 = 0;
        if (expectComma) {
            if (val->unit != Value::Operator || val->iValue != ',')
                goto failed;
            valueList->next();
            expectComma = false;
        }
        else {
            switch (propId) {
                case CSS_PROP_BACKGROUND_ATTACHMENT:
                    if (val->id == CSS_VAL_SCROLL || val->id == CSS_VAL_FIXED || val->id == CSS_VAL_LOCAL) {
                        currValue = new CSSPrimitiveValueImpl(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_COLOR:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        valueList->next();
                    break;
                case CSS_PROP_BACKGROUND_IMAGE: {
                    bool didParse=false;
                    currValue = parseBackgroundImage(didParse);
                    if (didParse)
                        valueList->next();
                    break;
                }
                case CSS_PROP_BACKGROUND_CLIP:
                case CSS_PROP_BACKGROUND_ORIGIN:
                    if (val->id == CSS_VAL_BORDER_BOX || val->id == CSS_VAL_PADDING_BOX || val->id == CSS_VAL_CONTENT_BOX) {
                        currValue = new CSSPrimitiveValueImpl(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_POSITION:
                    parseBackgroundPosition(currValue, currValue2);
                    // parseBackgroundPosition advances the valueList pointer
                    break;
                case CSS_PROP_BACKGROUND_POSITION_X: {
                    BackgroundPosKind pos;
                    currValue = parseBackgroundPositionXY(pos);
                    if (currValue) {
                        if (pos == BgPos_Y) {
                            delete currValue;
                            currValue = 0;
                        } else {
                            valueList->next();
                        }
                    }
                    break;
                }
                case CSS_PROP_BACKGROUND_POSITION_Y: {
                    BackgroundPosKind pos;
                    currValue = parseBackgroundPositionXY(pos);
                    if (currValue) {
                        if (pos == BgPos_X) {
                            delete currValue;
                            currValue = 0;
                        } else {
                            valueList->next();
                        }
                    }
                    break;
                }
                case CSS_PROP_BACKGROUND_REPEAT:
                    if (val->id >= CSS_VAL_REPEAT && val->id <= CSS_VAL_NO_REPEAT) {
                        currValue = new CSSPrimitiveValueImpl(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_SIZE:
                    currValue = parseBackgroundSize();
                    // parseBackgroundSize advances the valueList pointer
                    break;
            }

            if (!currValue) {
                goto failed;
            }

            // When parsing the 'background' shorthand property return the parsed value...
            if (inShorthand()) {
                retValue1 = currValue;
                if (currValue2) {
                    retValue2 = currValue2;
                }
                delete value; delete value2;
                return true;
            }

            // ...if not in shorthand, append to the list of value for the property
            // and expect a comma for the next value (if any)
            value->append(currValue);
            if (currValue2) {
                value2->append(currValue2);
            }
            expectComma = true;
        }
    }

    // Now return the value list
    if (value->length() > 0) {
        retValue1 = value;
        if (value2->length() > 0) {
            retValue2 = value2;
        } else {
            delete value2;
        }
        return true;
    }

failed:
    delete value; delete value2;
    return false;
}

bool CSSParser::parseShape( int propId, bool important )
{
    Value *value = valueList->current();
    ValueList *args = value->function->args;
    QString fname = qString( value->function->name ).toLower();
    //qDebug( "parseShape: fname: %d", fname.toLatin1().constData() );
    if ( fname != "rect(" || !args )
        return false;

    const int argsSize = args->size();
    // rect( t, r, b, l ) || rect( t r b l )
    if ( argsSize != 4 && argsSize != 7 )
        return false;
    RectImpl *rect = new RectImpl();
    bool valid = true;
    int i = 0;
    Value *a = args->current();
    while ( a ) {
        CSSPrimitiveValueImpl *length;
        if ( a->id == CSS_VAL_AUTO ) {
            length = new CSSPrimitiveValueImpl( CSS_VAL_AUTO );
        } else {
            valid  = validUnit( a, FLength, strict );
            if ( !valid )
                break;
            length = new CSSPrimitiveValueImpl( a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit );
        }

        if ( i == 0 )
            rect->setTop( length );
        else if ( i == 1 )
            rect->setRight( length );
        else if ( i == 2 )
            rect->setBottom( length );
        else
            rect->setLeft( length );
        a = args->next();
        if ( a && argsSize == 7 ) {
            if ( a->unit == Value::Operator && a->iValue == ',' ) {
                a = args->next();
            } else {
                valid = false;
                break;
            }
        }
        i++;
    }
    if ( valid ) {
        addProperty( propId, new CSSPrimitiveValueImpl( rect ), important );
        valueList->next();
        return true;
    }
    delete rect;
    return false;
}

// [ [ <'font-style'> || <'font-variant'> || <'font-weight'> ]? <'font-size'> [ / <'line-height'> ]? <'font-family'> ] |
// caption | icon | menu | message-box | small-caption | status-bar
bool CSSParser::parseFontShorthand(bool important)
{
    Value *value = valueList->current();
    if (valueList->size() == 1) {
        // Should be a system font identifier
        if (value->id >= CSS_VAL_CAPTION && value->id <= CSS_VAL_STATUS_BAR) {
            addProperty(CSS_PROP_FONT, new CSSPrimitiveValueImpl(value->id), important);
            return true;
        }
        return false;
    }
    CSSValueListImpl* family = 0;
    CSSPrimitiveValueImpl *style = 0, *variant = 0, *weight = 0, *size = 0, *lineHeight = 0;

    ShorthandScope scope(this, CSS_PROP_FONT);

    // optional font-style, font-variant and font-weight
    while (value) {
        //kWarning() << "got value" << value->id << "/" <<
        //(value->unit == CSSPrimitiveValue::CSS_STRING || value->unit == CSSPrimitiveValue::CSS_IDENT ? qString(value->string) : QString());
        const int id = value->id;

        if (id == CSS_VAL_NORMAL) {
            // do nothing, it's the initial value for all three
        } else if (id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE) {
            if (style) {
                goto invalid;
            }
            style = new CSSPrimitiveValueImpl(id);
        } else if (id == CSS_VAL_SMALL_CAPS) {
            if (variant) {
                goto invalid;
            }
            variant = new CSSPrimitiveValueImpl(id);
        } else if (int weightValueId = parseFontWeight(value, true)) {
            if (weight) {
                goto invalid;
            }
            weight = new CSSPrimitiveValueImpl(weightValueId);
        } else {
            break;
        }

        value = valueList->next();
    }

    if ( !value )
        goto invalid;

    // set undefined values to default
    if ( !style )
        style = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    if ( !variant )
        variant = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    if ( !weight )
        weight = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );

    //kWarning() << "parsed style, variant, weight:" << style->cssText() << variant->cssText() << weight->cssText();

    // now a font-size _must_ come
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if ( value->id >= CSS_VAL_XX_SMALL && value->id <= CSS_VAL_LARGER )
        size = new CSSPrimitiveValueImpl( value->id );
    else if (validUnit(value, FLength|FPercent|FNonNeg, strict)) {
        size = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
    }
    if (!size) {
        goto invalid;
    }
    //kWarning() << "parsed size:" << size->cssText();

    // now /line-height could come, next font-family _must_ come
    value = valueList->next();
    if (!value) {
        goto invalid;
    }

    if ( value->unit == Value::Operator && value->iValue == '/' ) {
        // line-height
        value = valueList->next();
        if ( !value )
            goto invalid;
        if ( value->id == CSS_VAL_NORMAL ) {
            // default value, nothing to do
        } else if ( validUnit( value, FNumber|FLength|FPercent|FNonNeg, strict ) ) {
            lineHeight = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
        } else {
            goto invalid;
        }
        value = valueList->next();
        if ( !value )
            goto invalid;
    }
    // if undefined set to default
    if ( !lineHeight )
        lineHeight = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    //kWarning() << "parsed line-height:" << lineHeight->cssText();

    // font-family _must_ come now
    family = parseFontFamily();

    if ( valueList->current() || !family )
        goto invalid;
    //kWarning() << "parsed family:" << family->cssText();

    addProperty( CSS_PROP_FONT_FAMILY,  family,     important );
    addProperty( CSS_PROP_FONT_STYLE,   style,      important );
    addProperty( CSS_PROP_FONT_VARIANT, variant,    important );
    addProperty( CSS_PROP_FONT_WEIGHT,  weight,     important );
    addProperty( CSS_PROP_FONT_SIZE,    size,       important );
    addProperty( CSS_PROP_LINE_HEIGHT,  lineHeight, important );
    return true;

 invalid:
    //kWarning() << " -> invalid";
    delete family;
    delete style;
    delete variant;
    delete weight;
    delete size;
    delete lineHeight;

    return false;
}

int CSSParser::parseFontWeight(Value *val, bool strict)
{
    const int valId = val->id;
    if (valId >= CSS_VAL_NORMAL && valId <= CSS_VAL_900) {
        // Valid primitive
        return valId;
    }
    if (validUnit(val, FInteger|FNonNeg, strict)) {
        int weight = static_cast<int>(val->fValue);
        if ((weight % 100)) {
            // Invalid
            return 0;
        }
        weight /= 100;
        if (weight >= 1 && weight <= 9) {
            return (CSS_VAL_100 + weight - 1);
        }
    }
    return 0;
}

CSSValueListImpl *CSSParser::parseFontFamily()
{
//     kDebug( 6080 ) << "CSSParser::parseFontFamily current=" << valueList->currentValue;
    CSSValueListImpl *list = new CSSValueListImpl(CSSValueListImpl::Comma);
    Value *value = valueList->current();
    QString currFace;

    while ( value ) {
//         kDebug( 6080 ) << "got value " << value->id << " / "
//                         << (value->unit == CSSPrimitiveValue::CSS_STRING ||
//                             value->unit == CSSPrimitiveValue::CSS_IDENT ? qString( value->string ) : QString() )
//                         << endl;
        Value* nextValue = valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == Value::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
                                 ((nextValue->id >= CSS_VAL_SERIF && nextValue->id <= CSS_VAL_MONOSPACE) ||
                                  (nextValue->unit == CSSPrimitiveValue::CSS_STRING ||
                                   nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        if (value->id == CSS_VAL_INHERIT && inShorthand() && currFace.isNull() && nextValBreaksFont) {
            // fail (#169610)
            delete list;
            return 0;
        }

        if (value->id >= CSS_VAL_SERIF && value->id <= CSS_VAL_MONOSPACE) {
            if (!currFace.isNull()) {
                currFace += ' ';
                currFace += qString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName) {
                if ( !currFace.isNull() ) {
                    list->append( new FontFamilyValueImpl( currFace ) );
                    currFace.clear();
                }
                list->append(new CSSPrimitiveValueImpl(value->id));
            }
            else {
                currFace = qString( value->string );
            }
        }
        else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            currFace.clear();
            list->append(new FontFamilyValueImpl(qString( value->string) ) );
        }
        else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (!currFace.isNull()) {
                currFace += ' ';
                currFace += qString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName) {
                if ( !currFace.isNull() ) {
                    list->append( new FontFamilyValueImpl( currFace ) );
                    currFace.clear();
                }
                list->append(new FontFamilyValueImpl( qString( value->string ) ) );
            }
            else {
                currFace = qString( value->string);
            }
        }
        else {
            //kDebug( 6080 ) << "invalid family part";
            break;
        }

        if (!nextValue)
            break;

        if (nextValBreaksFont) {
            value = valueList->next();
            if ( !currFace.isNull() )
                list->append( new FontFamilyValueImpl( currFace ) );
            currFace.clear();
        }
        else if (nextValIsFontName)
            value = nextValue;
        else
            break;
    }

    if ( !currFace.isNull() )
        list->append( new FontFamilyValueImpl( currFace ) );

    if ( !list->length() ) {
        delete list;
        list = 0;
    }
    return list;
}

bool CSSParser::parseFontFaceSrc()
{
    CSSValueListImpl* values = new CSSValueListImpl(CSSValueListImpl::Comma);
    Value* val;
    bool expectComma = false;
    bool allowFormat = false;
    bool failed = false;
    CSSFontFaceSrcValueImpl* uriValue = 0;
    while ((val = valueList->current())) {
        CSSFontFaceSrcValueImpl* parsedValue = 0;
        if (val->unit == CSSPrimitiveValue::CSS_URI && !expectComma && styleElement) {
            const DOMString uri = domString(val->string).parsedUrl();
            parsedValue = new CSSFontFaceSrcValueImpl(DOMString(KUrl(styleElement->baseURL(), uri.string()).url()), false /*local*/);
            uriValue = parsedValue;
            allowFormat = true;
            expectComma = true;
        } else if (val->unit == Value::Function) {
            // There are two allowed functions: local() and format().
            // For both we expect a string argument
            ValueList *args = val->function->args;
            if (args && args->size() == 1 &&
                (args->current()->unit == CSSPrimitiveValue::CSS_STRING ||
                 args->current()->unit == CSSPrimitiveValue::CSS_IDENT)) {
                if (!strcasecmp(domString(val->function->name), "local(") && !expectComma) {
                    expectComma = true;
                    allowFormat = false;
                    Value* a = args->current();
                    uriValue = 0;
                    parsedValue = new CSSFontFaceSrcValueImpl( domString( a->string ), true /*local src*/ );
                } else if (!strcasecmp(domString(val->function->name), "format(") && allowFormat && uriValue) {
                    expectComma = true;
                    allowFormat = false;
                    uriValue->setFormat( domString( args->current()->string ) );
                    uriValue = 0;
                    valueList->next();
                    continue;
                }
            }
        } else if (val->unit == Value::Operator && val->iValue == ',' && expectComma) {
            expectComma = false;
            allowFormat = false;
            uriValue = 0;
            valueList->next();
            continue;
        }

        if (parsedValue)
            values->append(parsedValue);
        else {
            failed = true;
            break;
        }
        valueList->next();
    }

    if (values->length() && !failed) {
        addProperty(CSS_PROP_SRC, values, important);
        valueList->next();
        return true;
    } else {
        delete values;
    }

    return false;
}
// [ <list-style-type> || <list-style-position> || <list-style-image> ]
bool CSSParser::parseListStyleShorthand(bool important)
{
    if (valueList->size() > 3) {
        // discard
        return false;
    }

    CSSValueImpl *type = 0;
    CSSValueImpl *position = 0;
    CSSValueImpl *image = 0;

    int numberOfNone = 0;
    Value *value = valueList->current();
    while (value) {
        const int valId = value->id;
        if (valId == CSS_VAL_NONE) {
            // just count
            ++numberOfNone;
        } else if (valId >= CSS_VAL_DISC && valId <= CSS_VAL__KHTML_CLOSE_QUOTE) {
            if (!type) {
                type = new CSSPrimitiveValueImpl(valId);
            } else {
                goto invalid;
            }
        } else if (valId == CSS_VAL_INSIDE || valId == CSS_VAL_OUTSIDE) {
            if (!position) {
                position = new CSSPrimitiveValueImpl(valId);
            } else {
                goto invalid;
            }
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            if (!image) {
                // ### allow string in non strict mode?
                if (styleElement) {
                    image = new CSSImageValueImpl(domString(value->string), styleElement);
                }
            } else {
                goto invalid;
            }
        } else {
            goto invalid;
        }
        value = valueList->next();
    }

    // Set whichever of 'list-style-type' and 'list-style-image' are not otherwise specified, to 'none'
    switch (numberOfNone) {
        case 0: {
            break;
        }
        case 1: {
            if (image && type) {
                goto invalid;
            }
            if (!image) {
                image = new CSSImageValueImpl();
            }
            if (!type) {
                type = new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            }
            break;
        }
        case 2: {
            if (image || type) {
                goto invalid;
            } else {
                image = new CSSImageValueImpl();
                type = new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            }
            break;
        }
        default: // numberOfNone == 3
            goto invalid;
    }

    // The shorthand is valid: fill-in any remaining properties with default value
    if (!type) {
        type = new CSSPrimitiveValueImpl(CSS_VAL_DISC);
    }
    if (!position) {
        position = new CSSPrimitiveValueImpl(CSS_VAL_OUTSIDE);
    }
    if (!image) {
        image = new CSSImageValueImpl();
    }
    addProperty(CSS_PROP_LIST_STYLE_TYPE, type, important);
    addProperty(CSS_PROP_LIST_STYLE_POSITION, position, important);
    addProperty(CSS_PROP_LIST_STYLE_IMAGE, image, important);

    return true;

invalid:
    delete type;
    delete position;
    delete image;

    return false;
}

bool CSSParser::parseColorParameters(Value* value, int* colorArray, bool parseAlpha)
{
    ValueList* args = value->function->args;
    Value* v = args->current();

    // Get the first value
    if (!validUnit(v, FInteger | FPercent, true))
        return false;
    bool isPercent = (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE);
    colorArray[0] = static_cast<int>(v->fValue * (isPercent ? 256.0 / 100.0 : 1.0));
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, (isPercent ? FPercent : FInteger), true))
            return false;
        colorArray[i] = static_cast<int>(v->fValue * (isPercent ? 256.0 / 100.0 : 1.0));
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, true))
            return false;
        colorArray[3] = static_cast<int>(std::max(0.0, std::min(1.0, v->fValue)) * 255); //krazy:exclude=qminmax
    }
    return true;
}

// CSS3 specification defines the format of a HSL color as
// hsl(<number>, <percent>, <percent>)
// and with alpha, the format is
// hsla(<number>, <percent>, <percent>, <number>)
// The first value, HUE, is in an angle with a value between 0 and 360
bool CSSParser::parseHSLParameters(Value* value, double* colorArray, bool parseAlpha)
{
    ValueList* args = value->function->args;
    Value* v = args->current();
    // Get the first value
    if (!validUnit(v, FInteger, true))
        return false;
    // normalize the Hue value and change it to be between 0 and 1.0
    colorArray[0] = (((static_cast<int>(v->fValue) % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FPercent, true))
            return false;
        colorArray[i] = std::max(0.0, std::min(100.0, v->fValue)) / 100.0; // needs to be value between 0 and 1.0, krazy:exclude=qminmax
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, true))
            return false;
        colorArray[3] = std::max(0.0, std::min(1.0, v->fValue)); //krazy:exclude=qminmax
    }
    return true;
}

static int hex2int(unsigned short c, bool* error)
{
    if ( c >= '0' && c <= '9' ) {
        return c - '0';
    } else if ( c >= 'A' && c <= 'F' ) {
        return 10 + c - 'A';
    } else if ( c >= 'a' && c <= 'f' ) {
        return 10 + c - 'a';
    } else {
        *error = true;
        return -1;
    }
}

static bool parseColor(int unit, const QString& name, QRgb& rgb, bool strict)
{
    int len = name.length();

    if ( !len )
        return false;

    if (unit == CSSPrimitiveValue::CSS_RGBCOLOR || !strict) {
        const unsigned short* c =
            reinterpret_cast<const unsigned short*>( name.unicode() );

        rgb = 0xff; // fixed alpha
        if ( len == 6 ) {
            // RRGGBB
            bool error = false;
            for ( int i = 0; i < 6; ++i, ++c )
                rgb = rgb << 4 | hex2int( *c, &error );
            if ( !error )
                return true;
        } else if ( len == 3 ) {
            // RGB, shortcut for RRGGBB
            bool error = false;
            for ( int i = 0; i < 3; ++i, ++c )
                rgb = rgb << 8 | 0x11 * hex2int( *c, &error );
            if ( !error )
                return true;
        }
    }

    if ( unit == CSSPrimitiveValue::CSS_IDENT ) {
        // try a little harder
        QColor tc;
        tc.setNamedColor(name.toLower());
        if ( tc.isValid() ) {
            rgb = tc.rgba();
            return true;
        }
    }

    return false;
}

CSSPrimitiveValueImpl *CSSParser::parseColor()
{
    return parseColorFromValue(valueList->current());
}

CSSPrimitiveValueImpl *CSSParser::parseColorFromValue(Value* value)
{
    QRgb c = khtml::transparentColor;
    if ( !strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&            // color: 000000 (quirk)
              value->fValue >= 0. && value->fValue < 1000000. ) {
        QString str = QString::asprintf( "%06d", (int)(value->fValue+.5) );
        if ( !::parseColor( CSSPrimitiveValue::CSS_RGBCOLOR, str, c, strict ) )
            return 0;
    }
    else if (value->unit == CSSPrimitiveValue::CSS_RGBCOLOR ||                 // color: #ff0000
             value->unit == CSSPrimitiveValue::CSS_IDENT ||                    // color: red || color: ff0000 (quirk)
             (!strict && value->unit == CSSPrimitiveValue::CSS_DIMENSION)) {   // color: 00ffff (quirk)
        if ( !::parseColor( value->unit, qString( value->string ), c, strict) )
            return 0;
    }
    else if ( value->unit == Value::Function &&
		value->function->args != 0 &&
                value->function->args->size() == 5 /* rgb + two commas */ &&
                qString( value->function->name ).toLower() == "rgb(" ) {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return 0;
        colorValues[0] = std::max( 0, std::min( 255, colorValues[0] ) );
        colorValues[1] = std::max( 0, std::min( 255, colorValues[1] ) );
        colorValues[2] = std::max( 0, std::min( 255, colorValues[2] ) );
        c = qRgb(colorValues[0], colorValues[1], colorValues[2]);
    } else if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 7 /* rgba + three commas */ &&
                domString(value->function->name).lower() == "rgba(") {
        int colorValues[4];
        if (!parseColorParameters(value, colorValues, true))
            return 0;
        colorValues[0] = std::max( 0, std::min( 255, colorValues[0] ) );
        colorValues[1] = std::max( 0, std::min( 255, colorValues[1] ) );
        colorValues[2] = std::max( 0, std::min( 255, colorValues[2] ) );
        c = qRgba(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
    } else if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 5 /* hsl + two commas */ &&
                domString(value->function->name).lower() == "hsl(") {
        double colorValues[3];
        if (!parseHSLParameters(value, colorValues, false))
            return 0;
        c = khtml::qRgbaFromHsla(colorValues[0], colorValues[1], colorValues[2], 1.0);
    } else if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 7 /* hsla + three commas */ &&
                domString(value->function->name).lower() == "hsla(") {
        double colorValues[4];
        if (!parseHSLParameters(value, colorValues, true))
            return 0;
        c = khtml::qRgbaFromHsla(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
    }
    else
        return 0;

    return new CSSPrimitiveValueImpl(c);
}

// This class tracks parsing state for shadow values.  If it goes out of scope (e.g., due to an early return)
// without the allowBreak bit being set, then it will clean up all of the objects and destroy them.
struct ShadowParseContext {
    ShadowParseContext()
    :values(0), x(0), y(0), blur(0), color(0),
     allowX(true), allowY(false), allowBlur(false), allowColor(true),
     allowBreak(true)
    {}

    ~ShadowParseContext() {
        if (!allowBreak) {
            delete values;
            delete x;
            delete y;
            delete blur;
            delete color;
        }
    }

    bool allowLength() { return allowX || allowY || allowBlur; }

    bool failed() { return allowBreak = false; }

    void commitValue() {
        // Handle the ,, case gracefully by doing nothing.
        if (x || y || blur || color) {
            if (!values)
                values = new CSSValueListImpl(CSSValueListImpl::Comma);

            // Construct the current shadow value and add it to the list.
            values->append(new ShadowValueImpl(x, y, blur, color));
        }

        // Now reset for the next shadow value.
        x = y = blur = color = 0;
        allowX = allowColor = allowBreak = true;
        allowY = allowBlur = false;
    }

    void commitLength(Value* v) {
        CSSPrimitiveValueImpl* val = new CSSPrimitiveValueImpl(v->fValue,
                                                               (CSSPrimitiveValue::UnitTypes)v->unit);
        if (allowX) {
            x = val;
            allowX = false; allowY = true; allowColor = false; allowBreak = false;
        }
        else if (allowY) {
            y = val;
            allowY = false; allowBlur = true; allowColor = true; allowBreak = true;
        }
        else if (allowBlur) {
            blur = val;
            allowBlur = false;
        }
	else
	    delete val;
    }

    void commitColor(CSSPrimitiveValueImpl* val) {
        color = val;
        allowColor = false;
        if (allowX)
            allowBreak = false;
        else
            allowBlur = false;
    }

    CSSValueListImpl* values;
    CSSPrimitiveValueImpl* x;
    CSSPrimitiveValueImpl* y;
    CSSPrimitiveValueImpl* blur;
    CSSPrimitiveValueImpl* color;

    bool allowX;
    bool allowY;
    bool allowBlur;
    bool allowColor;
    bool allowBreak;
};

bool CSSParser::parseShadow(int propId, bool important)
{
    ShadowParseContext context;
    Value* val;
    while ((val = valueList->current())) {
        // Check for a comma break first.
        if (val->unit == Value::Operator) {
            if (val->iValue != ',' || !context.allowBreak)
                // Other operators aren't legal or we aren't done with the current shadow
                // value.  Treat as invalid.
                return context.failed();

            // The value is good.  Commit it.
            context.commitValue();
        }
        // Check to see if we're a length.
        else if (validUnit(val, FLength, true)) {
            // We required a length and didn't get one. Invalid.
            if (!context.allowLength())
                return context.failed();

            // A length is allowed here.  Construct the value and add it.
            context.commitLength(val);
        }
        else {
            // The only other type of value that's ok is a color value.
            CSSPrimitiveValueImpl* parsedColor = 0;
            bool isColor = ((val->id >= CSS_VAL_AQUA && val->id <= CSS_VAL_WINDOWTEXT) ||
                            val->id == CSS_VAL_MENU ||
                           (val->id >= CSS_VAL_GREY && val->id <= CSS_VAL__KHTML_TEXT && !strict));
	    if (!context.allowColor)
                return context.failed();

            if (isColor)
               parsedColor = new CSSPrimitiveValueImpl(val->id);

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColorFromValue(val);

            if (!parsedColor)
                return context.failed();

            context.commitColor(parsedColor);
        }

        valueList->next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values->length()) {
            addProperty(propId, context.values, important);
            valueList->next();
            return true;
        }
    }

    return context.failed();
}

bool CSSParser::parseCounter(int propId, bool increment, bool important)
{
    enum { ID, VAL, COMMA } state = ID;

    CSSValueListImpl *list = new CSSValueListImpl;
    DOMString c;
    Value* val;
    while (true) {
        val = valueList->current();
        switch (state) {
            // Commas are not allowed according to the standard, but Opera allows them and being the only
            // other browser with counter support we need to match their behavior to work with current use
            case COMMA:
                state = ID;
                if (val && val->unit == Value::Operator && val->iValue == ',') {
                    valueList->next();
                    continue;
                }
                [[fallthrough]];
            case ID:
                if (val && val->unit == CSSPrimitiveValue::CSS_IDENT) {
                    c = qString(val->string);
                    state = VAL;
                    valueList->next();
                    continue;
                }
                break;
            case VAL: {
                short i = 0;
                if (val && val->unit == CSSPrimitiveValue::CSS_NUMBER) {
                    i = (short)val->fValue;
                    valueList->next();
                } else
                    i = (increment) ? 1 : 0;

                CounterActImpl *cv = new CounterActImpl(c,i);
                list->append(cv);
                state = COMMA;
                continue;
            }
        }
        break;
    }
    if(list->length() > 0) {
        addProperty( propId, list, important );
        return true;
    }
    delete list;
    return false;
}

static inline int yyerror( const char *str ) {
//    assert( 0 );
#ifdef CSS_DEBUG
    kDebug( 6080 ) << "CSS parse error " << str;
#else
    Q_UNUSED( str );
#endif
    return 1;
}

static const double dIntMax = INT_MAX;
#define END 0

#include "parser.h"

int DOM::CSSParser::lex( void *_yylval )
{
    YYSTYPE *yylval = (YYSTYPE *)_yylval;
    int token = lex();
    int length;
    unsigned short *t = text( &length );

#ifdef TOKEN_DEBUG
    qDebug("CSSTokenizer: got token %d: '%s'", token, token == END ? "" : QString( (QChar *)t, length ).toLatin1().constData() );
#endif
    switch( token ) {
    case '{':
        block_nesting++;
        break;
    case '}':
        if ( block_nesting )
            block_nesting--;
        break;
    case END:
        if ( block_nesting ) {
            block_nesting--;
            return '}';
        }
        break;
    case S:
    case SGML_CD:
    case INCLUDES:
    case DASHMATCH:
        break;

    case URI:
    case STRING:
    case IDENT:
    case NTH:
    case HASH:
    case HEXCOLOR:
    case DIMEN:
    case UNICODERANGE:
    case NOTFUNCTION:
    case FUNCTION:
        yylval->string.string = t;
        yylval->string.length = length;
        break;

    case IMPORT_SYM:
    case PAGE_SYM:
    case MEDIA_SYM:
    case FONT_FACE_SYM:
    case CHARSET_SYM:
    case NAMESPACE_SYM:

    case IMPORTANT_SYM:
        break;

    case QEMS:
        length--;
        [[fallthrough]];
    case GRADS:
    case DPCM:
        length--;
        [[fallthrough]];
    case DEGS:
    case RADS:
    case KHERZ:
    case DPI:
    case REMS:
        length--;
        [[fallthrough]];
    case MSECS:
    case HERZ:
    case EMS:
    case EXS:
    case CHS:
    case PXS:
    case CMS:
    case MMS:
    case INS:
    case PTS:
    case PCS:
        length--;
        [[fallthrough]];
    case SECS:
    case PERCENTAGE:
        length--;
        [[fallthrough]];
    case FLOAT:
    case INTEGER:
        yylval->val = std::min(QString((QChar *)t, length).toDouble(), dIntMax);
        //qDebug("value = %s, converted=%.2f", QString( (QChar *)t, length ).toLatin1().constData(), yylval->val );
        break;

    default:
        break;
    }

    return token;
}

static inline int toHex( char c ) {
    if ( '0' <= c && c <= '9' )
        return c - '0';
    if ( 'a' <= c && c <= 'f' )
        return c - 'a' + 10;
    if ( 'A' <= c && c<= 'F' )
        return c - 'A' + 10;
    return 0;
}

unsigned short *DOM::CSSParser::text(int *length)
{
    unsigned short *start = yytext;
    int l = yyleng;
    switch( yyTok ) {
    case STRING:
        l--;
        [[fallthrough]];
    case HASH:
    case HEXCOLOR:
        start++;
        l--;
        break;
    case URI:
        // "url("{w}{string}{w}")"
        // "url("{w}{url}{w}")"

        // strip "url(" and ")"
        start += 4;
        l -= 5;
        // strip {w}
        while ( l &&
                (*start == ' ' || *start == '\t' || *start == '\r' ||
                 *start == '\n' || *start == '\f' ) ) {
            start++; l--;
        }
        if ( *start == '"' || *start == '\'' ) {
            start++; l--;
        }
        while ( l &&
                (start[l-1] == ' ' || start[l-1] == '\t' || start[l-1] == '\r' ||
                 start[l-1] == '\n' || start[l-1] == '\f' ) ) {
            l--;
        }
        if ( l && (start[l-1] == '\"' || start[l-1] == '\'' ) )
             l--;

    default:
        break;
    }

    // process escapes
    unsigned short *out = start;
    unsigned short *escape = 0;

    for ( int i = 0; i < l; i++ ) {
        unsigned short *current = start+i;
        if ( escape == current - 1 ) {
            if ( ( *current >= '0' && *current <= '9' ) ||
                 ( *current >= 'a' && *current <= 'f' ) ||
                 ( *current >= 'A' && *current <= 'F' ) )
                continue;
            if ( yyTok == STRING &&
                 ( *current == '\n' || *current == '\r' || *current == '\f' ) ) {
                // ### handle \r\n case
                if ( *current != '\r' )
                    escape = 0;
                continue;
            }
            // in all other cases copy the char to output
            // ###
            *out++ = *current;
            escape = 0;
            continue;
        }
        if ( escape == current - 2 && yyTok == STRING &&
             *(current-1) == '\r' && *current == '\n' ) {
            escape = 0;
            continue;
        }
        if ( escape > current - 7 &&
             ( ( *current >= '0' && *current <= '9' ) ||
               ( *current >= 'a' && *current <= 'f' ) ||
               ( *current >= 'A' && *current <= 'F' ) ) )
                continue;
        if ( escape ) {
            // add escaped char
            int uc = 0;
            escape++;
            while ( escape < current ) {
//                 qDebug("toHex( %c = %x", (char)*escape, toHex( *escape ) );
                uc *= 16;
                uc += toHex( *escape );
                escape++;
            }
//             qDebug(" converting escape: string='%s', value=0x%x", QString( (QChar *)e, current-e ).toLatin1().constData(), uc );
            // can't handle chars outside utf16
            if ( uc > 0xffff )
                uc = 0xfffd;
            *(out++) = (unsigned short)uc;
            escape = 0;
            if ( *current == ' ' ||
                 *current == '\t' ||
                 *current == '\r' ||
                 *current == '\n' ||
                 *current == '\f' )
                continue;
        }
        if ( !escape && *current == '\\' ) {
            escape = current;
            continue;
        }
        *(out++) = *current;
    }
    if ( escape ) {
        // add escaped char
        int uc = 0;
        escape++;
        while ( escape < start+l ) {
            //                 qDebug("toHex( %c = %x", (char)*escape, toHex( *escape ) );
            uc *= 16;
            uc += toHex( *escape );
            escape++;
        }
        //             qDebug(" converting escape: string='%s', value=0x%x", QString( (QChar *)e, current-e ).toLatin1().constData(), uc );
        // can't handle chars outside utf16
        if ( uc > 0xffff )
            uc = 0xfffd;
        *(out++) = (unsigned short)uc;
    }

    *length = out - start;
    return start;
}

// When we reach the end of the input we switch over
// the lexer to this alternative buffer and keep it stuck here.
// (and as it contains nulls, flex will keep on reporting
//  end of buffer, and we will keep reseting the input
//  pointer to the beginning of this).
static unsigned short postEofBuf[2];

#define YY_DECL int DOM::CSSParser::lex()
#define yyconst const
typedef int yy_state_type;
typedef unsigned int YY_CHAR;
// this line makes sure we treat all Unicode chars correctly.
#define YY_SC_TO_UI(c) (c > 0xff ? 0xff : c)
#define YY_DO_BEFORE_ACTION \
        yytext = yy_bp; \
        yyleng = (int) (yy_cp - yy_bp); \
        yy_hold_char = *yy_cp; \
        *yy_cp = 0; \
        yy_c_buf_p = yy_cp;
#define YY_BREAK break;
#define ECHO qDebug( "%s", QString( (QChar *)yytext, yyleng ).toLatin1().constData() )
#define YY_RULE_SETUP
#define INITIAL 0
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
#define YY_START ((yy_start - 1) / 2)
#define yyterminate()\
    do { \
        if (yy_act == YY_END_OF_BUFFER) { \
            yy_c_buf_p = postEofBuf; \
            yy_hold_char = 0; /* first char of the postEndOf to 'restore' */ \
        } \
        yyTok = END; return yyTok; \
     } while (0)
#define YY_FATAL_ERROR(a) qFatal(a)
#define BEGIN yy_start = 1 + 2 *
#define COMMENT 1

#include "tokenizer.cpp"
// kate: indent-width 4; replace-tabs on; tab-width 4; space-indent on; hl c++;
