/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */
#ifndef INCLUDED_XMLOFF_FAMILIES_HXX
#define INCLUDED_XMLOFF_FAMILIES_HXX

#include <rtl/ustring.hxx>

/** These defines determine the unique ids for XML style-families
    used in the SvXMLAutoStylePoolP.
 */

inline constexpr OUStringLiteral XML_STYLE_FAMILY_PAGE_MASTER_NAME = u"page-layout";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_PAGE_MASTER_PREFIX = u"pm";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_TABLE_STYLES_NAME = u"table";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_TABLE_STYLES_PREFIX = u"ta";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_COLUMN_STYLES_NAME = u"table-column";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_COLUMN_STYLES_PREFIX  = u"co";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_ROW_STYLES_NAME = u"table-row";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_ROW_STYLES_PREFIX = u"ro";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_CELL_STYLES_NAME = u"table-cell";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_TABLE_CELL_STYLES_PREFIX = u"ce";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SD_GRAPHICS_NAME = u"graphic";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SD_GRAPHICS_PREFIX = u"gr";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SD_PRESENTATION_NAME = u"presentation";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SD_PRESENTATION_PREFIX = u"pr";
#define XML_STYLE_FAMILY_SD_POOL_NAME           u"default"
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SD_DRAWINGPAGE_NAME = u"drawing-page";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SD_DRAWINGPAGE_PREFIX = u"dp";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SCH_CHART_NAME = u"chart";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_SCH_CHART_PREFIX = u"ch";
inline constexpr OUStringLiteral XML_STYLE_FAMILY_CONTROL_PREFIX = u"ctrl";

enum class XmlStyleFamily
{
// Misc (Pool)
// reserved: 0..99
    DATA_STYLE             = 0,
    PAGE_MASTER            = 1,
    MASTER_PAGE            = 2,

// Text
// reserved: 100..199
    TEXT_PARAGRAPH         = 100,
    TEXT_TEXT              = 101,
    TEXT_LIST              = 102,
    TEXT_OUTLINE           = 103,
    TEXT_FOOTNOTECONFIG    = 105,
    TEXT_ENDNOTECONFIG     = 106,
    TEXT_SECTION           = 107,
    TEXT_FRAME             = 108, // export only
    TEXT_RUBY              = 109,
    TEXT_BIBLIOGRAPHYCONFIG = 110,
    TEXT_LINENUMBERINGCONFIG = 111,

// Table
// reserved: 200..299
    TABLE_TABLE            = 200,
    TABLE_COLUMN           = 202,
    TABLE_ROW              = 203,
    TABLE_CELL             = 204,
    TABLE_TEMPLATE_ID      = 205,

// Impress/Draw
// reserved: 300..399
    SD_GRAPHICS_ID         = 300,

    SD_PRESENTATION_ID     = 301,
// families for derived from SvXMLStyleContext
    SD_PAGEMASTERCONTEXT_ID        = 302,
    SD_PAGEMASTERSTYLECONTEXT_ID   = 306,
    SD_PRESENTATIONPAGELAYOUT_ID   = 303,
// family for draw pool
    SD_POOL_ID             = 304,
// family for presentation drawpage properties
    SD_DRAWINGPAGE_ID      = 305,

    SD_GRADIENT_ID         = 306,
    SD_HATCH_ID            = 307,
    SD_FILL_IMAGE_ID       = 308,
    SD_MARKER_ID           = 309,
    SD_STROKE_DASH_ID      = 310,

// Chart
// reserved: 400..499
    SCH_CHART_ID           = 400,

// Math
// reserved: 500..599


// Forms/Controls
// reserved 600..649
    CONTROL_ID             = 600,

};

#endif // INCLUDED_XMLOFF_FAMILIES_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
