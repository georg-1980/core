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

#include <svx/framelinkarray.hxx>

#include <math.h>
#include <vector>
#include <set>
#include <algorithm>
#include <tools/debug.hxx>
#include <tools/gen.hxx>
#include <vcl/canvastools.hxx>
#include <svx/sdr/primitive2d/sdrframeborderprimitive2d.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>

namespace svx::frame {

namespace {

class Cell
{
private:
    Style               maLeft;
    Style               maRight;
    Style               maTop;
    Style               maBottom;
    Style               maTLBR;
    Style               maBLTR;

public:
    sal_Int32                mnAddLeft;
    sal_Int32                mnAddRight;
    sal_Int32                mnAddTop;
    sal_Int32                mnAddBottom;

    SvxRotateMode       meRotMode;
    double              mfOrientation;

    bool                mbMergeOrig;
    bool                mbOverlapX;
    bool                mbOverlapY;

public:
    explicit            Cell();

    void SetStyleLeft(const Style& rStyle) { maLeft = rStyle; }
    void SetStyleRight(const Style& rStyle) { maRight = rStyle; }
    void SetStyleTop(const Style& rStyle) { maTop = rStyle; }
    void SetStyleBottom(const Style& rStyle) { maBottom = rStyle; }
    void SetStyleTLBR(const Style& rStyle) { maTLBR = rStyle; }
    void SetStyleBLTR(const Style& rStyle) { maBLTR = rStyle; }

    const Style& GetStyleLeft() const { return maLeft; }
    const Style& GetStyleRight() const { return maRight; }
    const Style& GetStyleTop() const { return maTop; }
    const Style& GetStyleBottom() const { return maBottom; }
    const Style& GetStyleTLBR() const { return maTLBR; }
    const Style& GetStyleBLTR() const { return maBLTR; }

    bool                IsMerged() const { return mbMergeOrig || mbOverlapX || mbOverlapY; }
    bool                IsRotated() const { return mfOrientation != 0.0; }

    void                MirrorSelfX();

    basegfx::B2DHomMatrix CreateCoordinateSystem(const Array& rArray, sal_Int32 nCol, sal_Int32 nRow, bool bExpandMerged) const;
};

}

typedef std::vector< Cell >     CellVec;

basegfx::B2DHomMatrix Cell::CreateCoordinateSystem(const Array& rArray, sal_Int32 nCol, sal_Int32 nRow, bool bExpandMerged) const
{
    basegfx::B2DHomMatrix aRetval;
    const basegfx::B2DRange aRange(rArray.GetCellRange(nCol, nRow, bExpandMerged));

    if(!aRange.isEmpty())
    {
        basegfx::B2DPoint aOrigin(aRange.getMinimum());
        basegfx::B2DVector aX(aRange.getWidth(), 0.0);
        basegfx::B2DVector aY(0.0, aRange.getHeight());

        if (IsRotated() && SvxRotateMode::SVX_ROTATE_MODE_STANDARD != meRotMode)
        {
            // when rotated, adapt values. Get Skew (cos/sin == 1/tan)
            const double fSkew(aY.getY() * (cos(mfOrientation) / sin(mfOrientation)));

            switch (meRotMode)
            {
            case SvxRotateMode::SVX_ROTATE_MODE_TOP:
                // shear Y-Axis
                aY.setX(-fSkew);
                break;
            case SvxRotateMode::SVX_ROTATE_MODE_CENTER:
                // shear origin half, Y full
                aOrigin.setX(aOrigin.getX() + (fSkew * 0.5));
                aY.setX(-fSkew);
                break;
            case SvxRotateMode::SVX_ROTATE_MODE_BOTTOM:
                // shear origin full, Y full
                aOrigin.setX(aOrigin.getX() + fSkew);
                aY.setX(-fSkew);
                break;
            default: // SvxRotateMode::SVX_ROTATE_MODE_STANDARD, already excluded above
                break;
            }
        }

        // use column vectors as coordinate axes, homogen column for translation
        aRetval = basegfx::utils::createCoordinateSystemTransform(aOrigin, aX, aY);
    }

    return aRetval;
}

Cell::Cell() :
    mnAddLeft( 0 ),
    mnAddRight( 0 ),
    mnAddTop( 0 ),
    mnAddBottom( 0 ),
    meRotMode(SvxRotateMode::SVX_ROTATE_MODE_STANDARD ),
    mfOrientation( 0.0 ),
    mbMergeOrig( false ),
    mbOverlapX( false ),
    mbOverlapY( false )
{
}

void Cell::MirrorSelfX()
{
    std::swap( maLeft, maRight );
    std::swap( mnAddLeft, mnAddRight );
    maLeft.MirrorSelf();
    maRight.MirrorSelf();
    mfOrientation = -mfOrientation;
}


static void lclRecalcCoordVec( std::vector<sal_Int32>& rCoords, const std::vector<sal_Int32>& rSizes )
{
    DBG_ASSERT( rCoords.size() == rSizes.size() + 1, "lclRecalcCoordVec - inconsistent vectors" );
    auto aCIt = rCoords.begin();
    for( const auto& rSize : rSizes )
    {
        *(aCIt + 1) = *aCIt + rSize;
        ++aCIt;
    }
}

static void lclSetMergedRange( CellVec& rCells, sal_Int32 nWidth, sal_Int32 nFirstCol, sal_Int32 nFirstRow, sal_Int32 nLastCol, sal_Int32 nLastRow )
{
    for( sal_Int32 nCol = nFirstCol; nCol <= nLastCol; ++nCol )
    {
        for( sal_Int32 nRow = nFirstRow; nRow <= nLastRow; ++nRow )
        {
            Cell& rCell = rCells[ nRow * nWidth + nCol ];
            rCell.mbMergeOrig = false;
            rCell.mbOverlapX = nCol > nFirstCol;
            rCell.mbOverlapY = nRow > nFirstRow;
        }
    }
    rCells[ nFirstRow * nWidth + nFirstCol ].mbMergeOrig = true;
}


const Style OBJ_STYLE_NONE;
const Cell OBJ_CELL_NONE;

struct ArrayImpl
{
    CellVec             maCells;
    std::vector<sal_Int32>   maWidths;
    std::vector<sal_Int32>   maHeights;
    mutable std::vector<sal_Int32>     maXCoords;
    mutable std::vector<sal_Int32>     maYCoords;
    sal_Int32           mnWidth;
    sal_Int32           mnHeight;
    sal_Int32           mnFirstClipCol;
    sal_Int32           mnFirstClipRow;
    sal_Int32           mnLastClipCol;
    sal_Int32           mnLastClipRow;
    mutable bool        mbXCoordsDirty;
    mutable bool        mbYCoordsDirty;
    bool                mbMayHaveCellRotation;

    explicit            ArrayImpl( sal_Int32 nWidth, sal_Int32 nHeight );

    bool         IsValidPos( sal_Int32 nCol, sal_Int32 nRow ) const
                            { return (nCol < mnWidth) && (nRow < mnHeight); }
    sal_Int32       GetIndex( sal_Int32 nCol, sal_Int32 nRow ) const
                            { return nRow * mnWidth + nCol; }

    const Cell&         GetCell( sal_Int32 nCol, sal_Int32 nRow ) const;
    Cell&               GetCellAcc( sal_Int32 nCol, sal_Int32 nRow );

    sal_Int32              GetMergedFirstCol( sal_Int32 nCol, sal_Int32 nRow ) const;
    sal_Int32              GetMergedFirstRow( sal_Int32 nCol, sal_Int32 nRow ) const;
    sal_Int32              GetMergedLastCol( sal_Int32 nCol, sal_Int32 nRow ) const;
    sal_Int32              GetMergedLastRow( sal_Int32 nCol, sal_Int32 nRow ) const;

    const Cell&         GetMergedOriginCell( sal_Int32 nCol, sal_Int32 nRow ) const;

    bool                IsMergedOverlappedLeft( sal_Int32 nCol, sal_Int32 nRow ) const;
    bool                IsMergedOverlappedRight( sal_Int32 nCol, sal_Int32 nRow ) const;
    bool                IsMergedOverlappedTop( sal_Int32 nCol, sal_Int32 nRow ) const;
    bool                IsMergedOverlappedBottom( sal_Int32 nCol, sal_Int32 nRow ) const;

    bool                IsInClipRange( sal_Int32 nCol, sal_Int32 nRow ) const;
    bool                IsColInClipRange( sal_Int32 nCol ) const;
    bool                IsRowInClipRange( sal_Int32 nRow ) const;

    sal_Int32           GetMirrorCol( sal_Int32 nCol ) const { return mnWidth - nCol - 1; }

    sal_Int32           GetColPosition( sal_Int32 nCol ) const;
    sal_Int32           GetRowPosition( sal_Int32 nRow ) const;

    bool                HasCellRotation() const;
};

ArrayImpl::ArrayImpl( sal_Int32 nWidth, sal_Int32 nHeight ) :
    mnWidth( nWidth ),
    mnHeight( nHeight ),
    mnFirstClipCol( 0 ),
    mnFirstClipRow( 0 ),
    mnLastClipCol( nWidth - 1 ),
    mnLastClipRow( nHeight - 1 ),
    mbXCoordsDirty( false ),
    mbYCoordsDirty( false ),
    mbMayHaveCellRotation( false )
{
    // default-construct all vectors
    maCells.resize( mnWidth * mnHeight );
    maWidths.resize( mnWidth, 0 );
    maHeights.resize( mnHeight, 0 );
    maXCoords.resize( mnWidth + 1, 0 );
    maYCoords.resize( mnHeight + 1, 0 );
}

const Cell& ArrayImpl::GetCell( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return IsValidPos( nCol, nRow ) ? maCells[ GetIndex( nCol, nRow ) ] : OBJ_CELL_NONE;
}

Cell& ArrayImpl::GetCellAcc( sal_Int32 nCol, sal_Int32 nRow )
{
    static Cell aDummy;
    return IsValidPos( nCol, nRow ) ? maCells[ GetIndex( nCol, nRow ) ] : aDummy;
}

sal_Int32 ArrayImpl::GetMergedFirstCol( sal_Int32 nCol, sal_Int32 nRow ) const
{
    sal_Int32 nFirstCol = nCol;
    while( (nFirstCol > 0) && GetCell( nFirstCol, nRow ).mbOverlapX ) --nFirstCol;
    return nFirstCol;
}

sal_Int32 ArrayImpl::GetMergedFirstRow( sal_Int32 nCol, sal_Int32 nRow ) const
{
    sal_Int32 nFirstRow = nRow;
    while( (nFirstRow > 0) && GetCell( nCol, nFirstRow ).mbOverlapY ) --nFirstRow;
    return nFirstRow;
}

sal_Int32 ArrayImpl::GetMergedLastCol( sal_Int32 nCol, sal_Int32 nRow ) const
{
    sal_Int32 nLastCol = nCol + 1;
    while( (nLastCol < mnWidth) && GetCell( nLastCol, nRow ).mbOverlapX ) ++nLastCol;
    return nLastCol - 1;
}

sal_Int32 ArrayImpl::GetMergedLastRow( sal_Int32 nCol, sal_Int32 nRow ) const
{
    sal_Int32 nLastRow = nRow + 1;
    while( (nLastRow < mnHeight) && GetCell( nCol, nLastRow ).mbOverlapY ) ++nLastRow;
    return nLastRow - 1;
}

const Cell& ArrayImpl::GetMergedOriginCell( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return GetCell( GetMergedFirstCol( nCol, nRow ), GetMergedFirstRow( nCol, nRow ) );
}

bool ArrayImpl::IsMergedOverlappedLeft( sal_Int32 nCol, sal_Int32 nRow ) const
{
    const Cell& rCell = GetCell( nCol, nRow );
    return rCell.mbOverlapX || (rCell.mnAddLeft > 0);
}

bool ArrayImpl::IsMergedOverlappedRight( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return GetCell( nCol + 1, nRow ).mbOverlapX || (GetCell( nCol, nRow ).mnAddRight > 0);
}

bool ArrayImpl::IsMergedOverlappedTop( sal_Int32 nCol, sal_Int32 nRow ) const
{
    const Cell& rCell = GetCell( nCol, nRow );
    return rCell.mbOverlapY || (rCell.mnAddTop > 0);
}

bool ArrayImpl::IsMergedOverlappedBottom( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return GetCell( nCol, nRow + 1 ).mbOverlapY || (GetCell( nCol, nRow ).mnAddBottom > 0);
}

bool ArrayImpl::IsColInClipRange( sal_Int32 nCol ) const
{
    return (mnFirstClipCol <= nCol) && (nCol <= mnLastClipCol);
}

bool ArrayImpl::IsRowInClipRange( sal_Int32 nRow ) const
{
    return (mnFirstClipRow <= nRow) && (nRow <= mnLastClipRow);
}

bool ArrayImpl::IsInClipRange( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return IsColInClipRange( nCol ) && IsRowInClipRange( nRow );
}

sal_Int32 ArrayImpl::GetColPosition( sal_Int32 nCol ) const
{
    if( mbXCoordsDirty )
    {
        lclRecalcCoordVec( maXCoords, maWidths );
        mbXCoordsDirty = false;
    }
    return maXCoords[ nCol ];
}

sal_Int32 ArrayImpl::GetRowPosition( sal_Int32 nRow ) const
{
    if( mbYCoordsDirty )
    {
        lclRecalcCoordVec( maYCoords, maHeights );
        mbYCoordsDirty = false;
    }
    return maYCoords[ nRow ];
}

bool ArrayImpl::HasCellRotation() const
{
    // check cell array
    for (const auto& aCell : maCells)
    {
        if (aCell.IsRotated())
        {
            return true;
        }
    }

    return false;
}

namespace {

class MergedCellIterator
{
public:
    explicit            MergedCellIterator( const Array& rArray, sal_Int32 nCol, sal_Int32 nRow );

    bool         Is() const { return (mnCol <= mnLastCol) && (mnRow <= mnLastRow); }
    sal_Int32       Col() const { return mnCol; }
    sal_Int32       Row() const { return mnRow; }

    MergedCellIterator& operator++();

private:
    sal_Int32              mnFirstCol;
    sal_Int32              mnFirstRow;
    sal_Int32              mnLastCol;
    sal_Int32              mnLastRow;
    sal_Int32              mnCol;
    sal_Int32              mnRow;
};

}

MergedCellIterator::MergedCellIterator( const Array& rArray, sal_Int32 nCol, sal_Int32 nRow )
{
    DBG_ASSERT( rArray.IsMerged( nCol, nRow ), "svx::frame::MergedCellIterator::MergedCellIterator - not in merged range" );
    rArray.GetMergedRange( mnFirstCol, mnFirstRow, mnLastCol, mnLastRow, nCol, nRow );
    mnCol = mnFirstCol;
    mnRow = mnFirstRow;
}

MergedCellIterator& MergedCellIterator::operator++()
{
    DBG_ASSERT( Is(), "svx::frame::MergedCellIterator::operator++() - already invalid" );
    if( ++mnCol > mnLastCol )
    {
        mnCol = mnFirstCol;
        ++mnRow;
    }
    return *this;
}


#define DBG_FRAME_CHECK( cond, funcname, error )        DBG_ASSERT( cond, "svx::frame::Array::" funcname " - " error )
#define DBG_FRAME_CHECK_COL( col, funcname )            DBG_FRAME_CHECK( (col) < GetColCount(), funcname, "invalid column index" )
#define DBG_FRAME_CHECK_ROW( row, funcname )            DBG_FRAME_CHECK( (row) < GetRowCount(), funcname, "invalid row index" )
#define DBG_FRAME_CHECK_COLROW( col, row, funcname )    DBG_FRAME_CHECK( ((col) < GetColCount()) && ((row) < GetRowCount()), funcname, "invalid cell index" )
#define DBG_FRAME_CHECK_COL_1( col, funcname )          DBG_FRAME_CHECK( (col) <= GetColCount(), funcname, "invalid column index" )
#define DBG_FRAME_CHECK_ROW_1( row, funcname )          DBG_FRAME_CHECK( (row) <= GetRowCount(), funcname, "invalid row index" )


#define CELL( col, row )        mxImpl->GetCell( col, row )
#define CELLACC( col, row )     mxImpl->GetCellAcc( col, row )
#define ORIGCELL( col, row )    mxImpl->GetMergedOriginCell( col, row )


Array::Array()
{
    Initialize( 0, 0 );
}

Array::~Array()
{
}

// array size and column/row indexes
void Array::Initialize( sal_Int32 nWidth, sal_Int32 nHeight )
{
    mxImpl.reset( new ArrayImpl( nWidth, nHeight ) );
}

sal_Int32 Array::GetColCount() const
{
    return mxImpl->mnWidth;
}

sal_Int32 Array::GetRowCount() const
{
    return mxImpl->mnHeight;
}

sal_Int32 Array::GetCellCount() const
{
    return mxImpl->maCells.size();
}

sal_Int32 Array::GetCellIndex( sal_Int32 nCol, sal_Int32 nRow, bool bRTL ) const
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "GetCellIndex" );
    if (bRTL)
        nCol = mxImpl->GetMirrorCol(nCol);
    return mxImpl->GetIndex( nCol, nRow );
}

// cell border styles
void Array::SetCellStyleLeft( sal_Int32 nCol, sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleLeft" );
    CELLACC( nCol, nRow ).SetStyleLeft(rStyle);
}

void Array::SetCellStyleRight( sal_Int32 nCol, sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleRight" );
    CELLACC( nCol, nRow ).SetStyleRight(rStyle);
}

void Array::SetCellStyleTop( sal_Int32 nCol, sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleTop" );
    CELLACC( nCol, nRow ).SetStyleTop(rStyle);
}

void Array::SetCellStyleBottom( sal_Int32 nCol, sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleBottom" );
    CELLACC( nCol, nRow ).SetStyleBottom(rStyle);
}

void Array::SetCellStyleTLBR( sal_Int32 nCol, sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleTLBR" );
    CELLACC( nCol, nRow ).SetStyleTLBR(rStyle);
}

void Array::SetCellStyleBLTR( sal_Int32 nCol, sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleBLTR" );
    CELLACC( nCol, nRow ).SetStyleBLTR(rStyle);
}

void Array::SetCellStyleDiag( sal_Int32 nCol, sal_Int32 nRow, const Style& rTLBR, const Style& rBLTR )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleDiag" );
    Cell& rCell = CELLACC( nCol, nRow );
    rCell.SetStyleTLBR(rTLBR);
    rCell.SetStyleBLTR(rBLTR);
}

void Array::SetColumnStyleLeft( sal_Int32 nCol, const Style& rStyle )
{
    DBG_FRAME_CHECK_COL( nCol, "SetColumnStyleLeft" );
    for( sal_Int32 nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
        SetCellStyleLeft( nCol, nRow, rStyle );
}

void Array::SetColumnStyleRight( sal_Int32 nCol, const Style& rStyle )
{
    DBG_FRAME_CHECK_COL( nCol, "SetColumnStyleRight" );
    for( sal_Int32 nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
        SetCellStyleRight( nCol, nRow, rStyle );
}

void Array::SetRowStyleTop( sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_ROW( nRow, "SetRowStyleTop" );
    for( sal_Int32 nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        SetCellStyleTop( nCol, nRow, rStyle );
}

void Array::SetRowStyleBottom( sal_Int32 nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_ROW( nRow, "SetRowStyleBottom" );
    for( sal_Int32 nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        SetCellStyleBottom( nCol, nRow, rStyle );
}

void Array::SetCellRotation(sal_Int32 nCol, sal_Int32 nRow, SvxRotateMode eRotMode, double fOrientation)
{
    DBG_FRAME_CHECK_COLROW(nCol, nRow, "SetCellRotation");
    Cell& rTarget = CELLACC(nCol, nRow);
    rTarget.meRotMode = eRotMode;
    rTarget.mfOrientation = fOrientation;

    if (!mxImpl->mbMayHaveCellRotation)
    {
        // activate once when a cell gets actually rotated to allow fast
        // answering HasCellRotation() calls
        mxImpl->mbMayHaveCellRotation = rTarget.IsRotated();
    }
}

bool Array::HasCellRotation() const
{
    if (!mxImpl->mbMayHaveCellRotation)
    {
        // never set, no need to check
        return false;
    }

    return mxImpl->HasCellRotation();
}

const Style& Array::GetCellStyleLeft( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // outside clipping rows or overlapped in merged cells: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) || mxImpl->IsMergedOverlappedLeft( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // left clipping border: always own left style
    if( nCol == mxImpl->mnFirstClipCol )
        return ORIGCELL( nCol, nRow ).GetStyleLeft();
    // right clipping border: always right style of left neighbor cell
    if( nCol == mxImpl->mnLastClipCol + 1 )
        return ORIGCELL( nCol - 1, nRow ).GetStyleRight();
    // outside clipping columns: invisible
    if( !mxImpl->IsColInClipRange( nCol ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own left style and right style of left neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleLeft(), ORIGCELL( nCol - 1, nRow ).GetStyleRight() );
}

const Style& Array::GetCellStyleRight( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // outside clipping rows or overlapped in merged cells: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) || mxImpl->IsMergedOverlappedRight( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // left clipping border: always left style of right neighbor cell
    if( nCol + 1 == mxImpl->mnFirstClipCol )
        return ORIGCELL( nCol + 1, nRow ).GetStyleLeft();
    // right clipping border: always own right style
    if( nCol == mxImpl->mnLastClipCol )
        return ORIGCELL( nCol, nRow ).GetStyleRight();
    // outside clipping columns: invisible
    if( !mxImpl->IsColInClipRange( nCol ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own right style and left style of right neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleRight(), ORIGCELL( nCol + 1, nRow ).GetStyleLeft() );
}

const Style& Array::GetCellStyleTop( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // outside clipping columns or overlapped in merged cells: invisible
    if( !mxImpl->IsColInClipRange( nCol ) || mxImpl->IsMergedOverlappedTop( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // top clipping border: always own top style
    if( nRow == mxImpl->mnFirstClipRow )
        return ORIGCELL( nCol, nRow ).GetStyleTop();
    // bottom clipping border: always bottom style of top neighbor cell
    if( nRow == mxImpl->mnLastClipRow + 1 )
        return ORIGCELL( nCol, nRow - 1 ).GetStyleBottom();
    // outside clipping rows: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own top style and bottom style of top neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleTop(), ORIGCELL( nCol, nRow - 1 ).GetStyleBottom() );
}

const Style& Array::GetCellStyleBottom( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // outside clipping columns or overlapped in merged cells: invisible
    if( !mxImpl->IsColInClipRange( nCol ) || mxImpl->IsMergedOverlappedBottom( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // top clipping border: always top style of bottom neighbor cell
    if( nRow + 1 == mxImpl->mnFirstClipRow )
        return ORIGCELL( nCol, nRow + 1 ).GetStyleTop();
    // bottom clipping border: always own bottom style
    if( nRow == mxImpl->mnLastClipRow )
        return ORIGCELL( nCol, nRow ).GetStyleBottom();
    // outside clipping rows: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own bottom style and top style of bottom neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleBottom(), ORIGCELL( nCol, nRow + 1 ).GetStyleTop() );
}

const Style& Array::GetCellStyleTLBR( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return CELL( nCol, nRow ).GetStyleTLBR();
}

const Style& Array::GetCellStyleBLTR( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return CELL( nCol, nRow ).GetStyleBLTR();
}

const Style& Array::GetCellStyleTL( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for top-left cell
    sal_Int32 nFirstCol = mxImpl->GetMergedFirstCol( nCol, nRow );
    sal_Int32 nFirstRow = mxImpl->GetMergedFirstRow( nCol, nRow );
    return ((nCol == nFirstCol) && (nRow == nFirstRow)) ?
        CELL( nFirstCol, nFirstRow ).GetStyleTLBR() : OBJ_STYLE_NONE;
}

const Style& Array::GetCellStyleBR( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for bottom-right cell
    sal_Int32 nLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
    sal_Int32 nLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
    return ((nCol == nLastCol) && (nRow == nLastRow)) ?
        CELL( mxImpl->GetMergedFirstCol( nCol, nRow ), mxImpl->GetMergedFirstRow( nCol, nRow ) ).GetStyleTLBR() : OBJ_STYLE_NONE;
}

const Style& Array::GetCellStyleBL( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for bottom-left cell
    sal_Int32 nFirstCol = mxImpl->GetMergedFirstCol( nCol, nRow );
    sal_Int32 nLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
    return ((nCol == nFirstCol) && (nRow == nLastRow)) ?
        CELL( nFirstCol, mxImpl->GetMergedFirstRow( nCol, nRow ) ).GetStyleBLTR() : OBJ_STYLE_NONE;
}

const Style& Array::GetCellStyleTR( sal_Int32 nCol, sal_Int32 nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for top-right cell
    sal_Int32 nFirstRow = mxImpl->GetMergedFirstRow( nCol, nRow );
    sal_Int32 nLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
    return ((nCol == nLastCol) && (nRow == nFirstRow)) ?
        CELL( mxImpl->GetMergedFirstCol( nCol, nRow ), nFirstRow ).GetStyleBLTR() : OBJ_STYLE_NONE;
}

// cell merging
void Array::SetMergedRange( sal_Int32 nFirstCol, sal_Int32 nFirstRow, sal_Int32 nLastCol, sal_Int32 nLastRow )
{
    DBG_FRAME_CHECK_COLROW( nFirstCol, nFirstRow, "SetMergedRange" );
    DBG_FRAME_CHECK_COLROW( nLastCol, nLastRow, "SetMergedRange" );
#if OSL_DEBUG_LEVEL >= 2
    {
        bool bFound = false;
        for( sal_Int32 nCurrCol = nFirstCol; !bFound && (nCurrCol <= nLastCol); ++nCurrCol )
            for( sal_Int32 nCurrRow = nFirstRow; !bFound && (nCurrRow <= nLastRow); ++nCurrRow )
                bFound = CELL( nCurrCol, nCurrRow ).IsMerged();
        DBG_FRAME_CHECK( !bFound, "SetMergedRange", "overlapping merged ranges" );
    }
#endif
    if( mxImpl->IsValidPos( nFirstCol, nFirstRow ) && mxImpl->IsValidPos( nLastCol, nLastRow ) )
        lclSetMergedRange( mxImpl->maCells, mxImpl->mnWidth, nFirstCol, nFirstRow, nLastCol, nLastRow );
}

void Array::SetAddMergedLeftSize( sal_Int32 nCol, sal_Int32 nRow, sal_Int32 nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedLeftSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedFirstCol( nCol, nRow ) == 0, "SetAddMergedLeftSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddLeft = nAddSize;
}

void Array::SetAddMergedRightSize( sal_Int32 nCol, sal_Int32 nRow, sal_Int32 nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedRightSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedLastCol( nCol, nRow ) + 1 == mxImpl->mnWidth, "SetAddMergedRightSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddRight = nAddSize;
}

void Array::SetAddMergedTopSize( sal_Int32 nCol, sal_Int32 nRow, sal_Int32 nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedTopSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedFirstRow( nCol, nRow ) == 0, "SetAddMergedTopSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddTop = nAddSize;
}

void Array::SetAddMergedBottomSize( sal_Int32 nCol, sal_Int32 nRow, sal_Int32 nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedBottomSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedLastRow( nCol, nRow ) + 1 == mxImpl->mnHeight, "SetAddMergedBottomSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddBottom = nAddSize;
}

bool Array::IsMerged( sal_Int32 nCol, sal_Int32 nRow ) const
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "IsMerged" );
    return CELL( nCol, nRow ).IsMerged();
}

void Array::GetMergedOrigin( sal_Int32& rnFirstCol, sal_Int32& rnFirstRow, sal_Int32 nCol, sal_Int32 nRow ) const
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "GetMergedOrigin" );
    rnFirstCol = mxImpl->GetMergedFirstCol( nCol, nRow );
    rnFirstRow = mxImpl->GetMergedFirstRow( nCol, nRow );
}

void Array::GetMergedRange( sal_Int32& rnFirstCol, sal_Int32& rnFirstRow,
        sal_Int32& rnLastCol, sal_Int32& rnLastRow, sal_Int32 nCol, sal_Int32 nRow ) const
{
    GetMergedOrigin( rnFirstCol, rnFirstRow, nCol, nRow );
    rnLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
    rnLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
}

// clipping
void Array::SetClipRange( sal_Int32 nFirstCol, sal_Int32 nFirstRow, sal_Int32 nLastCol, sal_Int32 nLastRow )
{
    DBG_FRAME_CHECK_COLROW( nFirstCol, nFirstRow, "SetClipRange" );
    DBG_FRAME_CHECK_COLROW( nLastCol, nLastRow, "SetClipRange" );
    mxImpl->mnFirstClipCol = nFirstCol;
    mxImpl->mnFirstClipRow = nFirstRow;
    mxImpl->mnLastClipCol = nLastCol;
    mxImpl->mnLastClipRow = nLastRow;
}

// cell coordinates
void Array::SetXOffset( sal_Int32 nXOffset )
{
    mxImpl->maXCoords[ 0 ] = nXOffset;
    mxImpl->mbXCoordsDirty = true;
}

void Array::SetYOffset( sal_Int32 nYOffset )
{
    mxImpl->maYCoords[ 0 ] = nYOffset;
    mxImpl->mbYCoordsDirty = true;
}

void Array::SetColWidth( sal_Int32 nCol, sal_Int32 nWidth )
{
    DBG_FRAME_CHECK_COL( nCol, "SetColWidth" );
    mxImpl->maWidths[ nCol ] = nWidth;
    mxImpl->mbXCoordsDirty = true;
}

void Array::SetRowHeight( sal_Int32 nRow, sal_Int32 nHeight )
{
    DBG_FRAME_CHECK_ROW( nRow, "SetRowHeight" );
    mxImpl->maHeights[ nRow ] = nHeight;
    mxImpl->mbYCoordsDirty = true;
}

void Array::SetAllColWidths( sal_Int32 nWidth )
{
    std::fill( mxImpl->maWidths.begin(), mxImpl->maWidths.end(), nWidth );
    mxImpl->mbXCoordsDirty = true;
}

void Array::SetAllRowHeights( sal_Int32 nHeight )
{
    std::fill( mxImpl->maHeights.begin(), mxImpl->maHeights.end(), nHeight );
    mxImpl->mbYCoordsDirty = true;
}

sal_Int32 Array::GetColPosition( sal_Int32 nCol ) const
{
    DBG_FRAME_CHECK_COL_1( nCol, "GetColPosition" );
    return mxImpl->GetColPosition( nCol );
}

sal_Int32 Array::GetRowPosition( sal_Int32 nRow ) const
{
    DBG_FRAME_CHECK_ROW_1( nRow, "GetRowPosition" );
    return mxImpl->GetRowPosition( nRow );
}

sal_Int32 Array::GetColWidth( sal_Int32 nFirstCol, sal_Int32 nLastCol ) const
{
    DBG_FRAME_CHECK_COL( nFirstCol, "GetColWidth" );
    DBG_FRAME_CHECK_COL( nLastCol, "GetColWidth" );
    return GetColPosition( nLastCol + 1 ) - GetColPosition( nFirstCol );
}

sal_Int32 Array::GetRowHeight( sal_Int32 nFirstRow, sal_Int32 nLastRow ) const
{
    DBG_FRAME_CHECK_ROW( nFirstRow, "GetRowHeight" );
    DBG_FRAME_CHECK_ROW( nLastRow, "GetRowHeight" );
    return GetRowPosition( nLastRow + 1 ) - GetRowPosition( nFirstRow );
}

sal_Int32 Array::GetWidth() const
{
    return GetColPosition( mxImpl->mnWidth ) - GetColPosition( 0 );
}

sal_Int32 Array::GetHeight() const
{
    return GetRowPosition( mxImpl->mnHeight ) - GetRowPosition( 0 );
}

basegfx::B2DRange Array::GetCellRange( sal_Int32 nCol, sal_Int32 nRow, bool bExpandMerged ) const
{
    if(bExpandMerged)
    {
        // get the Range of the fully expanded cell (if merged)
        const sal_Int32 nFirstCol(mxImpl->GetMergedFirstCol( nCol, nRow ));
        const sal_Int32 nFirstRow(mxImpl->GetMergedFirstRow( nCol, nRow ));
        const sal_Int32 nLastCol(mxImpl->GetMergedLastCol( nCol, nRow ));
        const sal_Int32 nLastRow(mxImpl->GetMergedLastRow( nCol, nRow ));
        const Point aPoint( GetColPosition( nFirstCol ), GetRowPosition( nFirstRow ) );
        const Size aSize( GetColWidth( nFirstCol, nLastCol ) + 1, GetRowHeight( nFirstRow, nLastRow ) + 1 );
        tools::Rectangle aRect(aPoint, aSize);

        // adjust rectangle for partly visible merged cells
        const Cell& rCell = CELL( nCol, nRow );

        if( rCell.IsMerged() )
        {
            // not *sure* what exactly this is good for,
            // it is just a hard set extension at merged cells,
            // probably *should* be included in the above extended
            // GetColPosition/GetColWidth already. This might be
            // added due to GetColPosition/GetColWidth not working
            // correctly over PageChanges (if used), but not sure.
            aRect.AdjustLeft( -(rCell.mnAddLeft) );
            aRect.AdjustRight(rCell.mnAddRight );
            aRect.AdjustTop( -(rCell.mnAddTop) );
            aRect.AdjustBottom(rCell.mnAddBottom );
        }

        return vcl::unotools::b2DRectangleFromRectangle(aRect);
    }
    else
    {
        const Point aPoint( GetColPosition( nCol ), GetRowPosition( nRow ) );
        const Size aSize( GetColWidth( nCol, nCol ) + 1, GetRowHeight( nRow, nRow ) + 1 );
        const tools::Rectangle aRect(aPoint, aSize);

        return vcl::unotools::b2DRectangleFromRectangle(aRect);
    }
}

// mirroring
void Array::MirrorSelfX()
{
    CellVec aNewCells;
    aNewCells.reserve( GetCellCount() );

    sal_Int32 nCol, nRow;
    for( nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
    {
        for( nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        {
            aNewCells.push_back( CELL( mxImpl->GetMirrorCol( nCol ), nRow ) );
            aNewCells.back().MirrorSelfX();
        }
    }
    for( nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
    {
        for( nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        {
            if( CELL( nCol, nRow ).mbMergeOrig )
            {
                sal_Int32 nLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
                sal_Int32 nLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
                lclSetMergedRange( aNewCells, mxImpl->mnWidth,
                    mxImpl->GetMirrorCol( nLastCol ), nRow,
                    mxImpl->GetMirrorCol( nCol ), nLastRow );
            }
        }
    }
    mxImpl->maCells.swap( aNewCells );

    std::reverse( mxImpl->maWidths.begin(), mxImpl->maWidths.end() );
    mxImpl->mbXCoordsDirty = true;
}

// drawing
static void HelperCreateHorizontalEntry(
    const Array& rArray,
    const Style& rStyle,
    sal_Int32 col,
    sal_Int32 row,
    const basegfx::B2DPoint& rOrigin,
    const basegfx::B2DVector& rX,
    const basegfx::B2DVector& rY,
    drawinglayer::primitive2d::SdrFrameBorderDataVector& rData,
    bool bUpper,
    const Color* pForceColor)
{
    // prepare SdrFrameBorderData
    rData.emplace_back(
        bUpper ? rOrigin : basegfx::B2DPoint(rOrigin + rY),
        rX,
        rStyle,
        pForceColor);
    drawinglayer::primitive2d::SdrFrameBorderData& rInstance(rData.back());

    // get involved styles at start
    const Style& rStartFromTR(rArray.GetCellStyleBL( col, row - 1 ));
    const Style& rStartLFromT(rArray.GetCellStyleLeft( col, row - 1 ));
    const Style& rStartLFromL(rArray.GetCellStyleTop( col - 1, row ));
    const Style& rStartLFromB(rArray.GetCellStyleLeft( col, row ));
    const Style& rStartFromBR(rArray.GetCellStyleTL( col, row ));

    rInstance.addSdrConnectStyleData(true, rStartFromTR, rX - rY, false);
    rInstance.addSdrConnectStyleData(true, rStartLFromT, -rY, true);
    rInstance.addSdrConnectStyleData(true, rStartLFromL, -rX, true);
    rInstance.addSdrConnectStyleData(true, rStartLFromB, rY, false);
    rInstance.addSdrConnectStyleData(true, rStartFromBR, rX + rY, false);

    // get involved styles at end
    const Style& rEndFromTL(rArray.GetCellStyleBR( col, row - 1 ));
    const Style& rEndRFromT(rArray.GetCellStyleRight( col, row - 1 ));
    const Style& rEndRFromR(rArray.GetCellStyleTop( col + 1, row ));
    const Style& rEndRFromB(rArray.GetCellStyleRight( col, row ));
    const Style& rEndFromBL(rArray.GetCellStyleTR( col, row ));

    rInstance.addSdrConnectStyleData(false, rEndFromTL, -rX - rY, true);
    rInstance.addSdrConnectStyleData(false, rEndRFromT, -rY, true);
    rInstance.addSdrConnectStyleData(false, rEndRFromR, rX, false);
    rInstance.addSdrConnectStyleData(false, rEndRFromB, rY, false);
    rInstance.addSdrConnectStyleData(false, rEndFromBL, rY - rX, true);
}

static void HelperCreateVerticalEntry(
    const Array& rArray,
    const Style& rStyle,
    sal_Int32 col,
    sal_Int32 row,
    const basegfx::B2DPoint& rOrigin,
    const basegfx::B2DVector& rX,
    const basegfx::B2DVector& rY,
    drawinglayer::primitive2d::SdrFrameBorderDataVector& rData,
    bool bLeft,
    const Color* pForceColor)
{
    // prepare SdrFrameBorderData
    rData.emplace_back(
        bLeft ? rOrigin : basegfx::B2DPoint(rOrigin + rX),
        rY,
        rStyle,
        pForceColor);
    drawinglayer::primitive2d::SdrFrameBorderData& rInstance(rData.back());

    // get involved styles at start
    const Style& rStartFromBL(rArray.GetCellStyleTR( col - 1, row ));
    const Style& rStartTFromL(rArray.GetCellStyleTop( col - 1, row ));
    const Style& rStartTFromT(rArray.GetCellStyleLeft( col, row - 1 ));
    const Style& rStartTFromR(rArray.GetCellStyleTop( col, row ));
    const Style& rStartFromBR(rArray.GetCellStyleTL( col, row ));

    rInstance.addSdrConnectStyleData(true, rStartFromBR, rX + rY, false);
    rInstance.addSdrConnectStyleData(true, rStartTFromR, rX, false);
    rInstance.addSdrConnectStyleData(true, rStartTFromT, -rY, true);
    rInstance.addSdrConnectStyleData(true, rStartTFromL, -rX, true);
    rInstance.addSdrConnectStyleData(true, rStartFromBL, rY - rX, true);

    // get involved styles at end
    const Style& rEndFromTL(rArray.GetCellStyleBR( col - 1, row ));
    const Style& rEndBFromL(rArray.GetCellStyleBottom( col - 1, row ));
    const Style& rEndBFromB(rArray.GetCellStyleLeft( col, row + 1 ));
    const Style& rEndBFromR(rArray.GetCellStyleBottom( col, row ));
    const Style& rEndFromTR(rArray.GetCellStyleBL( col, row ));

    rInstance.addSdrConnectStyleData(false, rEndFromTR, rX - rY, false);
    rInstance.addSdrConnectStyleData(false, rEndBFromR, rX, false);
    rInstance.addSdrConnectStyleData(false, rEndBFromB, rY, false);
    rInstance.addSdrConnectStyleData(false, rEndBFromL, -rX, true);
    rInstance.addSdrConnectStyleData(false, rEndFromTL, -rY - rX, true);
}

drawinglayer::primitive2d::Primitive2DContainer Array::CreateB2DPrimitiveRange(
    sal_Int32 nFirstCol, sal_Int32 nFirstRow, sal_Int32 nLastCol, sal_Int32 nLastRow,
    const Color* pForceColor ) const
{
    DBG_FRAME_CHECK_COLROW( nFirstCol, nFirstRow, "CreateB2DPrimitiveRange" );
    DBG_FRAME_CHECK_COLROW( nLastCol, nLastRow, "CreateB2DPrimitiveRange" );

    // It may be necessary to extend the loop ranges by one cell to the outside,
    // when possible. This is needed e.g. when there is in Calc a Cell with an
    // upper CellBorder using DoubleLine and that is right/left connected upwards
    // to also DoubleLine. These upper DoubleLines will be extended to meet the
    // lower of the upper CellBorder and thus have graphical parts that are
    // displayed one cell below and right/left of the target cell - analog to
    // other examples in all other directions.
    // It would be possible to explicitly test this (if possible by indices at all)
    // looping and testing the styles in the outer cells to detect this, but since
    // for other usages (e.g. UI) usually nFirstRow==0 and nLastRow==GetRowCount()-1
    // (and analog for Col) it is okay to just expand the range when available.
    // Do *not* change nFirstRow/nLastRow due to these needed to the boolean tests
    // below (!)
    // Checked usages, this method is used in Calc EditView/Print/Export stuff and
    // in UI (Dialog), not for Writer Tables and Draw/Impress tables. All usages
    // seem okay with this change, so I will add it.
    const sal_Int32 nStartRow(nFirstRow > 0 ? nFirstRow - 1 : nFirstRow);
    const sal_Int32 nEndRow(nLastRow < GetRowCount() - 1 ? nLastRow + 1 : nLastRow);
    const sal_Int32 nStartCol(nFirstCol > 0 ? nFirstCol - 1 : nFirstCol);
    const sal_Int32 nEndCol(nLastCol < GetColCount() - 1 ? nLastCol + 1 : nLastCol);

    // prepare SdrFrameBorderDataVector
    std::shared_ptr<drawinglayer::primitive2d::SdrFrameBorderDataVector> aData(
        std::make_shared<drawinglayer::primitive2d::SdrFrameBorderDataVector>());

    // remember for which merged cells crossed lines were already created. To
    // do so, hold the sal_Int32 cell index in a set for fast check
    std::set< sal_Int32 > aMergedCells;

    for (sal_Int32 nRow(nStartRow); nRow <= nEndRow; ++nRow)
    {
        for (sal_Int32 nCol(nStartCol); nCol <= nEndCol; ++nCol)
        {
            // get Cell and CoordinateSystem (*only* for this Cell, do *not* expand for
            // merged cells (!)), check if used (non-empty vectors)
            const Cell& rCell(CELL(nCol, nRow));
            basegfx::B2DHomMatrix aCoordinateSystem(rCell.CreateCoordinateSystem(*this, nCol, nRow, false));
            basegfx::B2DVector aX(basegfx::utils::getColumn(aCoordinateSystem, 0));
            basegfx::B2DVector aY(basegfx::utils::getColumn(aCoordinateSystem, 1));

            // get needed local values
            basegfx::B2DPoint aOrigin(basegfx::utils::getColumn(aCoordinateSystem, 2));
            const bool bOverlapX(rCell.mbOverlapX);
            const bool bFirstCol(nCol == nFirstCol);

            // handle rotation: If cell is rotated, handle lower/right edge inside
            // this local geometry due to the created CoordinateSystem already representing
            // the needed transformations.
            const bool bRotated(rCell.IsRotated());

            // Additionally avoid double-handling by suppressing handling when self not rotated,
            // but above/left is rotated and thus already handled. Two directly connected
            // rotated will paint/create both edges, they might be rotated differently.
            const bool bSuppressLeft(!bRotated && nCol > nFirstCol && CELL(nCol - 1, nRow).IsRotated());
            const bool bSuppressAbove(!bRotated && nRow > nFirstRow && CELL(nCol, nRow - 1).IsRotated());

            if(!aX.equalZero() && !aY.equalZero())
            {
                // additionally needed local values
                const bool bOverlapY(rCell.mbOverlapY);
                const bool bLastCol(nCol == nLastCol);
                const bool bFirstRow(nRow == nFirstRow);
                const bool bLastRow(nRow == nLastRow);

                // create upper line for this Cell
                if ((!bOverlapY         // true for first line in merged cells or cells
                    || bFirstRow)       // true for non_Calc usages of this tooling
                    && !bSuppressAbove) // true when above is not rotated, so edge is already handled (see bRotated)
                {
                    // get CellStyle - method will take care to get the correct one, e.g.
                    // for merged cells (it uses ORIGCELL that works with topLeft's of these)
                    const Style& rTop(GetCellStyleTop(nCol, nRow));

                    if(rTop.IsUsed())
                    {
                        HelperCreateHorizontalEntry(*this, rTop, nCol, nRow, aOrigin, aX, aY, *aData, true, pForceColor);
                    }
                }

                // create lower line for this Cell
                if (bLastRow       // true for non_Calc usages of this tooling
                    || bRotated)   // true if cell is rotated, handle lower edge in local geometry
                {
                    const Style& rBottom(GetCellStyleBottom(nCol, nRow));

                    if(rBottom.IsUsed())
                    {
                        HelperCreateHorizontalEntry(*this, rBottom, nCol, nRow + 1, aOrigin, aX, aY, *aData, false, pForceColor);
                    }
                }

                // create left line for this Cell
                if ((!bOverlapX         // true for first column in merged cells or cells
                    || bFirstCol)       // true for non_Calc usages of this tooling
                    && !bSuppressLeft)  // true when left is not rotated, so edge is already handled (see bRotated)
                {
                    const Style& rLeft(GetCellStyleLeft(nCol, nRow));

                    if(rLeft.IsUsed())
                    {
                        HelperCreateVerticalEntry(*this, rLeft, nCol, nRow, aOrigin, aX, aY, *aData, true, pForceColor);
                    }
                }

                // create right line for this Cell
                if (bLastCol        // true for non_Calc usages of this tooling
                    || bRotated)    // true if cell is rotated, handle right edge in local geometry
                {
                    const Style& rRight(GetCellStyleRight(nCol, nRow));

                    if(rRight.IsUsed())
                    {
                        HelperCreateVerticalEntry(*this, rRight, nCol + 1, nRow, aOrigin, aX, aY, *aData, false, pForceColor);
                    }
                }

                // check for crossed lines, these need special treatment, especially
                // for merged cells, see below
                const Style& rTLBR(GetCellStyleTLBR(nCol, nRow));
                const Style& rBLTR(GetCellStyleBLTR(nCol, nRow));

                if(rTLBR.IsUsed() || rBLTR.IsUsed())
                {
                    bool bContinue(true);

                    if(rCell.IsMerged())
                    {
                        // first check if this merged cell was already handled. To do so,
                        // calculate and use the index of the TopLeft cell
                        const sal_Int32 _nMergedFirstCol(mxImpl->GetMergedFirstCol(nCol, nRow));
                        const sal_Int32 _nMergedFirstRow(mxImpl->GetMergedFirstRow(nCol, nRow));
                        const sal_Int32 nIndexOfMergedCell(mxImpl->GetIndex(_nMergedFirstCol, _nMergedFirstRow));
                        bContinue = (aMergedCells.end() == aMergedCells.find(nIndexOfMergedCell));

                        if(bContinue)
                        {
                            // not found, add now to mark as handled
                            aMergedCells.insert(nIndexOfMergedCell);

                            // when merged, get extended coordinate system and derived values
                            // for the full range of this merged cell
                            aCoordinateSystem = rCell.CreateCoordinateSystem(*this, nCol, nRow, true);
                            aX = basegfx::utils::getColumn(aCoordinateSystem, 0);
                            aY = basegfx::utils::getColumn(aCoordinateSystem, 1);
                            aOrigin = basegfx::utils::getColumn(aCoordinateSystem, 2);
                        }
                    }

                    if(bContinue)
                    {
                        if(rTLBR.IsUsed())
                        {
                            /// top-left and bottom-right Style Tables
                            aData->emplace_back(
                                aOrigin,
                                aX + aY,
                                rTLBR,
                                pForceColor);
                            drawinglayer::primitive2d::SdrFrameBorderData& rInstance(aData->back());

                            /// Fill top-left Style Table
                            const Style& rTLFromRight(GetCellStyleTop(nCol, nRow));
                            const Style& rTLFromBottom(GetCellStyleLeft(nCol, nRow));

                            rInstance.addSdrConnectStyleData(true, rTLFromRight, aX, false);
                            rInstance.addSdrConnectStyleData(true, rTLFromBottom, aY, false);

                            /// Fill bottom-right Style Table
                            const Style& rBRFromBottom(GetCellStyleRight(nCol, nRow));
                            const Style& rBRFromLeft(GetCellStyleBottom(nCol, nRow));

                            rInstance.addSdrConnectStyleData(false, rBRFromBottom, -aY, true);
                            rInstance.addSdrConnectStyleData(false, rBRFromLeft, -aX, true);
                        }

                        if(rBLTR.IsUsed())
                        {
                            /// bottom-left and top-right Style Tables
                            aData->emplace_back(
                                aOrigin + aY,
                                aX - aY,
                                rBLTR,
                                pForceColor);
                            drawinglayer::primitive2d::SdrFrameBorderData& rInstance(aData->back());

                            /// Fill bottom-left Style Table
                            const Style& rBLFromTop(GetCellStyleLeft(nCol, nRow));
                            const Style& rBLFromBottom(GetCellStyleBottom(nCol, nRow));

                            rInstance.addSdrConnectStyleData(true, rBLFromTop, -aY, true);
                            rInstance.addSdrConnectStyleData(true, rBLFromBottom, aX, false);

                            /// Fill top-right Style Table
                            const Style& rTRFromLeft(GetCellStyleTop(nCol, nRow));
                            const Style& rTRFromBottom(GetCellStyleRight(nCol, nRow));

                            rInstance.addSdrConnectStyleData(false, rTRFromLeft, -aX, true);
                            rInstance.addSdrConnectStyleData(false, rTRFromBottom, aY, false);
                        }
                    }
                }
            }
            else
            {
                // create left line for this Cell
                if ((!bOverlapX         // true for first column in merged cells or cells
                    || bFirstCol)       // true for non_Calc usages of this tooling
                    && !bSuppressLeft)  // true when left is not rotated, so edge is already handled (see bRotated)
                {
                    const Style& rLeft(GetCellStyleLeft(nCol, nRow));

                    if (rLeft.IsUsed())
                    {
                        HelperCreateVerticalEntry(*this, rLeft, nCol, nRow, aOrigin, aX, aY, *aData, true, pForceColor);
                    }
                }
            }
        }
    }

    // create instance of SdrFrameBorderPrimitive2D if
    // SdrFrameBorderDataVector is used
    drawinglayer::primitive2d::Primitive2DContainer aSequence;

    if(!aData->empty())
    {
        aSequence.append(
            drawinglayer::primitive2d::Primitive2DReference(
                new drawinglayer::primitive2d::SdrFrameBorderPrimitive2D(
                    aData,
                    true)));    // force visualization to minimal one discrete unit (pixel)
    }

    return aSequence;
}

drawinglayer::primitive2d::Primitive2DContainer Array::CreateB2DPrimitiveArray() const
{
    drawinglayer::primitive2d::Primitive2DContainer aPrimitives;

    if (mxImpl->mnWidth && mxImpl->mnHeight)
    {
        aPrimitives = CreateB2DPrimitiveRange(0, 0, mxImpl->mnWidth - 1, mxImpl->mnHeight - 1, nullptr);
    }

    return aPrimitives;
}

#undef ORIGCELL
#undef CELLACC
#undef CELL
#undef DBG_FRAME_CHECK_ROW_1
#undef DBG_FRAME_CHECK_COL_1
#undef DBG_FRAME_CHECK_COLROW
#undef DBG_FRAME_CHECK_ROW
#undef DBG_FRAME_CHECK_COL
#undef DBG_FRAME_CHECK

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
