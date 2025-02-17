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

#include <AxisHelper.hxx>
#include <DiagramHelper.hxx>
#include <ChartTypeHelper.hxx>
#include <AxisIndexDefines.hxx>
#include <LinePropertiesHelper.hxx>
#include <servicenames_coosystems.hxx>
#include <DataSeriesHelper.hxx>
#include <Scaling.hxx>
#include <ChartModel.hxx>
#include <ChartModelHelper.hxx>
#include <DataSourceHelper.hxx>
#include <ReferenceSizeProvider.hxx>
#include <ExplicitCategoriesProvider.hxx>
#include <unonames.hxx>

#include <unotools/saveopt.hxx>

#include <com/sun/star/chart/ChartAxisPosition.hpp>
#include <com/sun/star/chart2/AxisType.hpp>
#include <com/sun/star/chart2/XCoordinateSystemContainer.hpp>
#include <com/sun/star/chart2/XChartTypeContainer.hpp>
#include <com/sun/star/chart2/XDataSeriesContainer.hpp>
#include <com/sun/star/chart2/data/XDataSource.hpp>

#include <sal/log.hxx>

#include <com/sun/star/lang/XServiceName.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <comphelper/sequence.hxx>
#include <tools/diagnose_ex.h>

#include <map>

namespace chart
{
using namespace ::com::sun::star;
using namespace ::com::sun::star::chart2;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;

Reference< chart2::XScaling > AxisHelper::createLinearScaling()
{
    return new LinearScaling( 1.0, 0.0 );
}

Reference< chart2::XScaling > AxisHelper::createLogarithmicScaling( double fBase )
{
    return new LogarithmicScaling( fBase );
}

ScaleData AxisHelper::createDefaultScale()
{
    ScaleData aScaleData;
    aScaleData.AxisType = chart2::AxisType::REALNUMBER;
    aScaleData.AutoDateAxis = true;
    aScaleData.ShiftedCategoryPosition = false;
    Sequence< SubIncrement > aSubIncrements{ SubIncrement() };
    aScaleData.IncrementData.SubIncrements = aSubIncrements;
    return aScaleData;
}

void AxisHelper::removeExplicitScaling( ScaleData& rScaleData )
{
    uno::Any aEmpty;
    rScaleData.Minimum = rScaleData.Maximum = rScaleData.Origin = aEmpty;
    rScaleData.Scaling = nullptr;
    ScaleData aDefaultScale( createDefaultScale() );
    rScaleData.IncrementData = aDefaultScale.IncrementData;
    rScaleData.TimeIncrement = aDefaultScale.TimeIncrement;
}

bool AxisHelper::isLogarithmic( const Reference< XScaling >& xScaling )
{
    Reference< lang::XServiceName > xServiceName( xScaling, uno::UNO_QUERY );
    return xServiceName.is()
        && xServiceName->getServiceName() == "com.sun.star.chart2.LogarithmicScaling";
}

chart2::ScaleData AxisHelper::getDateCheckedScale( const Reference< chart2::XAxis >& xAxis, ChartModel& rModel )
{
    ScaleData aScale = xAxis->getScaleData();
    Reference< chart2::XCoordinateSystem > xCooSys( ChartModelHelper::getFirstCoordinateSystem( rModel ) );
    if( aScale.AutoDateAxis && aScale.AxisType == AxisType::CATEGORY )
    {
        sal_Int32 nDimensionIndex=0; sal_Int32 nAxisIndex=0;
        AxisHelper::getIndicesForAxis(xAxis, xCooSys, nDimensionIndex, nAxisIndex );
        bool bChartTypeAllowsDateAxis = ChartTypeHelper::isSupportingDateAxis( AxisHelper::getChartTypeByIndex( xCooSys, 0 ), nDimensionIndex );
        if( bChartTypeAllowsDateAxis )
            aScale.AxisType = AxisType::DATE;
    }
    if( aScale.AxisType == AxisType::DATE )
    {
        ExplicitCategoriesProvider aExplicitCategoriesProvider( xCooSys, rModel );
        if( !aExplicitCategoriesProvider.isDateAxis() )
            aScale.AxisType = AxisType::CATEGORY;
    }
    return aScale;
}

void AxisHelper::checkDateAxis( chart2::ScaleData& rScale, ExplicitCategoriesProvider* pExplicitCategoriesProvider, bool bChartTypeAllowsDateAxis )
{
    if( rScale.AutoDateAxis && rScale.AxisType == AxisType::CATEGORY && bChartTypeAllowsDateAxis )
    {
        rScale.AxisType = AxisType::DATE;
        removeExplicitScaling( rScale );
    }
    if( rScale.AxisType == AxisType::DATE && (!pExplicitCategoriesProvider || !pExplicitCategoriesProvider->isDateAxis()) )
    {
        rScale.AxisType = AxisType::CATEGORY;
        removeExplicitScaling( rScale );
    }
}

sal_Int32 AxisHelper::getExplicitNumberFormatKeyForAxis(
                  const Reference< chart2::XAxis >& xAxis
                , const Reference< chart2::XCoordinateSystem > & xCorrespondingCoordinateSystem
                , const Reference<chart2::XChartDocument>& xChartDoc
                , bool bSearchForParallelAxisIfNothingIsFound )
{
    sal_Int32 nNumberFormatKey(0);
    sal_Int32 nAxisIndex = 0;
    sal_Int32 nDimensionIndex = 1;
    AxisHelper::getIndicesForAxis( xAxis, xCorrespondingCoordinateSystem, nDimensionIndex, nAxisIndex );
    Reference<util::XNumberFormatsSupplier> const xNumberFormatsSupplier(xChartDoc, uno::UNO_QUERY);

    Reference< beans::XPropertySet > xProp( xAxis, uno::UNO_QUERY );
    if (!xProp.is())
        return 0;

    bool bLinkToSource = true;
    xProp->getPropertyValue(CHART_UNONAME_LINK_TO_SRC_NUMFMT) >>= bLinkToSource;
    xProp->getPropertyValue(CHART_UNONAME_NUMFMT) >>= nNumberFormatKey;

    if (bLinkToSource)
    {
        bool bFormatSet = false;
        //check whether we have a percent scale -> use percent format
        ChartModel* pModel = nullptr;
        if( xNumberFormatsSupplier.is() )
        {
            pModel = dynamic_cast<ChartModel*>( xChartDoc.get() );
            assert(pModel);
        }
        if (pModel)
        {
            ScaleData aData = AxisHelper::getDateCheckedScale( xAxis, *pModel );
            if( aData.AxisType==AxisType::PERCENT )
            {
                sal_Int32 nPercentFormat = DiagramHelper::getPercentNumberFormat( xNumberFormatsSupplier );
                if( nPercentFormat != -1 )
                {
                    nNumberFormatKey = nPercentFormat;
                    bFormatSet = true;
                }
            }
            else if( aData.AxisType==AxisType::DATE )
            {
                if( aData.Categories.is() )
                {
                    Reference< data::XDataSequence > xSeq( aData.Categories->getValues());
                    if( xSeq.is() && !( xChartDoc.is() && xChartDoc->hasInternalDataProvider()) )
                        nNumberFormatKey = xSeq->getNumberFormatKeyByIndex( -1 );
                    else
                        nNumberFormatKey = DiagramHelper::getDateNumberFormat( xNumberFormatsSupplier );
                    bFormatSet = true;
                }
            }
            else if( xChartDoc.is() && xChartDoc->hasInternalDataProvider() && nDimensionIndex == 0 ) //maybe date axis
            {
                Reference< chart2::XDiagram > xDiagram( xChartDoc->getFirstDiagram() );
                if( DiagramHelper::isSupportingDateAxis( xDiagram ) )
                {
                    nNumberFormatKey = DiagramHelper::getDateNumberFormat( xNumberFormatsSupplier );
                }
                else
                {
                    Reference< data::XDataSource > xSource( DataSourceHelper::getUsedData( xChartDoc ) );
                    if( xSource.is() )
                    {
                        std::vector< Reference< chart2::data::XLabeledDataSequence > > aXValues(
                            DataSeriesHelper::getAllDataSequencesByRole( xSource->getDataSequences(), "values-x" ) );
                        if( aXValues.empty() )
                        {
                            Reference< data::XLabeledDataSequence > xCategories( DiagramHelper::getCategoriesFromDiagram( xDiagram ) );
                            if( xCategories.is() )
                            {
                                Reference< data::XDataSequence > xSeq( xCategories->getValues());
                                if( xSeq.is() )
                                {
                                    bool bHasValidDoubles = false;
                                    double fTest=0.0;
                                    Sequence< uno::Any > aCats( xSeq->getData() );
                                    sal_Int32 nCount = aCats.getLength();
                                    for( sal_Int32 i = 0; i < nCount; ++i )
                                    {
                                        if( (aCats[i]>>=fTest) && !std::isnan(fTest) )
                                        {
                                            bHasValidDoubles=true;
                                            break;
                                        }
                                    }
                                    if( bHasValidDoubles )
                                        nNumberFormatKey = DiagramHelper::getDateNumberFormat( xNumberFormatsSupplier );
                                }
                            }
                        }
                    }
                }
                bFormatSet = true;
            }
        }

        if( !bFormatSet )
        {
            std::map< sal_Int32, sal_Int32 > aKeyMap;
            bool bNumberFormatKeyFoundViaAttachedData = false;

            try
            {
                Reference< XChartTypeContainer > xCTCnt( xCorrespondingCoordinateSystem, uno::UNO_QUERY_THROW );
                OUString aRoleToMatch;
                if( nDimensionIndex == 0 )
                    aRoleToMatch = "values-x";
                const Sequence< Reference< XChartType > > aChartTypes( xCTCnt->getChartTypes());
                for( Reference< XChartType > const & chartType : aChartTypes )
                {
                    if( nDimensionIndex != 0 )
                        aRoleToMatch = ChartTypeHelper::getRoleOfSequenceForYAxisNumberFormatDetection( chartType );
                    Reference< XDataSeriesContainer > xDSCnt( chartType, uno::UNO_QUERY_THROW );
                    const Sequence< Reference< XDataSeries > > aDataSeriesSeq( xDSCnt->getDataSeries());
                    for( Reference< chart2::XDataSeries > const & xDataSeries : aDataSeriesSeq )
                    {
                        Reference< data::XDataSource > xSource( xDataSeries, uno::UNO_QUERY_THROW );

                        if( nDimensionIndex == 1 )
                        {
                            //only take those series into account that are attached to this axis
                            sal_Int32 nAttachedAxisIndex = DataSeriesHelper::getAttachedAxisIndex(xDataSeries);
                            if( nAttachedAxisIndex != nAxisIndex )
                                continue;
                        }

                        Reference< data::XLabeledDataSequence > xLabeledSeq(
                            DataSeriesHelper::getDataSequenceByRole( xSource, aRoleToMatch ) );

                        if( !xLabeledSeq.is() && nDimensionIndex==0 )
                        {
                            ScaleData aData = xAxis->getScaleData();
                            xLabeledSeq = aData.Categories;
                        }

                        if( xLabeledSeq.is() )
                        {
                            Reference< data::XDataSequence > xSeq( xLabeledSeq->getValues());
                            if( xSeq.is() )
                            {
                                sal_Int32 nKey = xSeq->getNumberFormatKeyByIndex( -1 );
                                // increase frequency
                                aKeyMap[ nKey ] ++;
                            }
                        }
                    }
                }
            }
            catch( const uno::Exception & )
            {
                DBG_UNHANDLED_EXCEPTION("chart2");
            }

            if( ! aKeyMap.empty())
            {
                sal_Int32 nMaxFreq = 0;
                // find most frequent key
                for (auto const& elem : aKeyMap)
                {
                    SAL_INFO(
                        "chart2.tools",
                        "NumberFormatKey " << elem.first << " appears "
                            << elem.second << " times");
                    // all values must at least be 1
                    if( elem.second > nMaxFreq )
                    {
                        nNumberFormatKey = elem.first;
                        bNumberFormatKeyFoundViaAttachedData = true;
                        nMaxFreq = elem.second;
                    }
                }
            }

            if( bSearchForParallelAxisIfNothingIsFound )
            {
                //no format is set to this axis and no data is set to this axis
                //--> try to obtain the format from the parallel y-axis
                if( !bNumberFormatKeyFoundViaAttachedData && nDimensionIndex == 1 )
                {
                    sal_Int32 nParallelAxisIndex = (nAxisIndex==1) ?0 :1;
                    Reference< XAxis > xParallelAxis( AxisHelper::getAxis( 1, nParallelAxisIndex, xCorrespondingCoordinateSystem ) );
                    nNumberFormatKey = AxisHelper::getExplicitNumberFormatKeyForAxis(xParallelAxis, xCorrespondingCoordinateSystem, xChartDoc, false);
                }
            }
        }
    }

    return nNumberFormatKey;
}

Reference< XAxis > AxisHelper::createAxis(
          sal_Int32 nDimensionIndex
        , sal_Int32 nAxisIndex // 0==main or 1==secondary axis
        , const Reference< XCoordinateSystem >& xCooSys
        , const Reference< uno::XComponentContext > & xContext
        , ReferenceSizeProvider * pRefSizeProvider )
{
    if( !xContext.is() || !xCooSys.is() )
        return nullptr;
    if( nDimensionIndex >= xCooSys->getDimension() )
        return nullptr;

    Reference< XAxis > xAxis( xContext->getServiceManager()->createInstanceWithContext(
                    "com.sun.star.chart2.Axis", xContext ), uno::UNO_QUERY );

    OSL_ASSERT( xAxis.is());
    if( xAxis.is())
    {
        xCooSys->setAxisByDimension( nDimensionIndex, xAxis, nAxisIndex );

        if( nAxisIndex>0 )//when inserting secondary axes copy some things from the main axis
        {
            css::chart::ChartAxisPosition eNewAxisPos( css::chart::ChartAxisPosition_END );

            Reference< XAxis > xMainAxis( xCooSys->getAxisByDimension( nDimensionIndex, 0 ) );
            if( xMainAxis.is() )
            {
                ScaleData aScale = xAxis->getScaleData();
                ScaleData aMainScale = xMainAxis->getScaleData();

                aScale.AxisType = aMainScale.AxisType;
                aScale.AutoDateAxis = aMainScale.AutoDateAxis;
                aScale.Categories = aMainScale.Categories;
                aScale.Orientation = aMainScale.Orientation;
                aScale.ShiftedCategoryPosition = aMainScale.ShiftedCategoryPosition;

                xAxis->setScaleData( aScale );

                //ensure that the second axis is not placed on the main axis
                Reference< beans::XPropertySet > xMainProp( xMainAxis, uno::UNO_QUERY );
                if( xMainProp.is() )
                {
                    css::chart::ChartAxisPosition eMainAxisPos( css::chart::ChartAxisPosition_ZERO );
                    xMainProp->getPropertyValue("CrossoverPosition") >>= eMainAxisPos;
                    if( eMainAxisPos == css::chart::ChartAxisPosition_END )
                        eNewAxisPos = css::chart::ChartAxisPosition_START;
                }
            }

            Reference< beans::XPropertySet > xProp( xAxis, uno::UNO_QUERY );
            if( xProp.is() )
                xProp->setPropertyValue("CrossoverPosition", uno::Any(eNewAxisPos) );
        }

        Reference< beans::XPropertySet > xProp( xAxis, uno::UNO_QUERY );
        if( xProp.is() ) try
        {
            // set correct initial AutoScale
            if( pRefSizeProvider )
                pRefSizeProvider->setValuesAtPropertySet( xProp );
        }
        catch( const uno::Exception& )
        {
            TOOLS_WARN_EXCEPTION("chart2", "" );
        }
    }
    return xAxis;
}

Reference< XAxis > AxisHelper::createAxis( sal_Int32 nDimensionIndex, bool bMainAxis
                , const Reference< chart2::XDiagram >& xDiagram
                , const Reference< uno::XComponentContext >& xContext
                , ReferenceSizeProvider * pRefSizeProvider )
{
    OSL_ENSURE( xContext.is(), "need a context to create an axis" );
    if( !xContext.is() )
        return nullptr;

    sal_Int32 nAxisIndex = bMainAxis ? MAIN_AXIS_INDEX : SECONDARY_AXIS_INDEX;
    Reference< XCoordinateSystem > xCooSys = AxisHelper::getCoordinateSystemByIndex( xDiagram, 0 );

    // create axis
    return AxisHelper::createAxis(
        nDimensionIndex, nAxisIndex, xCooSys, xContext, pRefSizeProvider );
}

void AxisHelper::showAxis( sal_Int32 nDimensionIndex, bool bMainAxis
                , const Reference< chart2::XDiagram >& xDiagram
                , const Reference< uno::XComponentContext >& xContext
                , ReferenceSizeProvider * pRefSizeProvider )
{
    if( !xDiagram.is() )
        return;

    bool bNewAxisCreated = false;
    Reference< XAxis > xAxis( AxisHelper::getAxis( nDimensionIndex, bMainAxis, xDiagram ) );
    if( !xAxis.is() && xContext.is() )
    {
        // create axis
        bNewAxisCreated = true;
        xAxis.set( AxisHelper::createAxis( nDimensionIndex, bMainAxis, xDiagram, xContext, pRefSizeProvider ) );
    }

    OSL_ASSERT( xAxis.is());
    if( !bNewAxisCreated ) //default is true already if created
        AxisHelper::makeAxisVisible( xAxis );
}

void AxisHelper::showGrid( sal_Int32 nDimensionIndex, sal_Int32 nCooSysIndex, bool bMainGrid
                , const Reference< XDiagram >& xDiagram )
{
    if( !xDiagram.is() )
        return;

    Reference< XCoordinateSystem > xCooSys = AxisHelper::getCoordinateSystemByIndex( xDiagram, nCooSysIndex );
    if(!xCooSys.is())
        return;

    Reference< XAxis > xAxis( AxisHelper::getAxis( nDimensionIndex, MAIN_AXIS_INDEX, xCooSys ) );
    if(!xAxis.is())
    {
        //hhhh todo create axis without axis visibility
    }
    if(!xAxis.is())
        return;

    if( bMainGrid )
        AxisHelper::makeGridVisible( xAxis->getGridProperties() );
    else
    {
        const Sequence< Reference< beans::XPropertySet > > aSubGrids( xAxis->getSubGridProperties() );
        for( auto const & i : aSubGrids )
            AxisHelper::makeGridVisible( i );
    }
}

void AxisHelper::makeAxisVisible( const Reference< XAxis >& xAxis )
{
    Reference< beans::XPropertySet > xProps( xAxis, uno::UNO_QUERY );
    if( xProps.is() )
    {
        xProps->setPropertyValue( "Show", uno::Any( true ) );
        LinePropertiesHelper::SetLineVisible( xProps );
        xProps->setPropertyValue( "DisplayLabels", uno::Any( true ) );
    }
}

void AxisHelper::makeGridVisible( const Reference< beans::XPropertySet >& xGridProperties )
{
    if( xGridProperties.is() )
    {
        xGridProperties->setPropertyValue( "Show", uno::Any( true ) );
        LinePropertiesHelper::SetLineVisible( xGridProperties );
    }
}

void AxisHelper::hideAxis( sal_Int32 nDimensionIndex, bool bMainAxis
                , const Reference< XDiagram >& xDiagram )
{
    AxisHelper::makeAxisInvisible( AxisHelper::getAxis( nDimensionIndex, bMainAxis, xDiagram ) );
}

void AxisHelper::makeAxisInvisible( const Reference< XAxis >& xAxis )
{
    Reference< beans::XPropertySet > xProps( xAxis, uno::UNO_QUERY );
    if( xProps.is() )
    {
        xProps->setPropertyValue( "Show", uno::Any( false ) );
    }
}

void AxisHelper::hideAxisIfNoDataIsAttached( const Reference< XAxis >& xAxis, const Reference< XDiagram >& xDiagram )
{
    //axis is hidden if no data is attached anymore but data is available
    bool bOtherSeriesAttachedToThisAxis = false;
    std::vector< Reference< chart2::XDataSeries > > aSeriesVector( DiagramHelper::getDataSeriesFromDiagram( xDiagram ) );
    for (auto const& series : aSeriesVector)
    {
        uno::Reference< chart2::XAxis > xCurrentAxis = DiagramHelper::getAttachedAxis(series, xDiagram );
        if( xCurrentAxis==xAxis )
        {
            bOtherSeriesAttachedToThisAxis = true;
            break;
        }
    }
    if(!bOtherSeriesAttachedToThisAxis && !aSeriesVector.empty() )
        AxisHelper::makeAxisInvisible( xAxis );
}

void AxisHelper::hideGrid( sal_Int32 nDimensionIndex, sal_Int32 nCooSysIndex, bool bMainGrid
                , const Reference< XDiagram >& xDiagram )
{
    if( !xDiagram.is() )
        return;

    Reference< XCoordinateSystem > xCooSys = AxisHelper::getCoordinateSystemByIndex( xDiagram, nCooSysIndex );
    if(!xCooSys.is())
        return;

    Reference< XAxis > xAxis( AxisHelper::getAxis( nDimensionIndex, MAIN_AXIS_INDEX, xCooSys ) );
    if(!xAxis.is())
        return;

    if( bMainGrid )
        AxisHelper::makeGridInvisible( xAxis->getGridProperties() );
    else
    {
        const Sequence< Reference< beans::XPropertySet > > aSubGrids( xAxis->getSubGridProperties() );
        for( auto const & i : aSubGrids)
            AxisHelper::makeGridInvisible( i );
    }
}

void AxisHelper::makeGridInvisible( const Reference< beans::XPropertySet >& xGridProperties )
{
    if( xGridProperties.is() )
    {
        xGridProperties->setPropertyValue( "Show", uno::Any( false ) );
    }
}

bool AxisHelper::isGridShown( sal_Int32 nDimensionIndex, sal_Int32 nCooSysIndex, bool bMainGrid
                , const Reference< css::chart2::XDiagram >& xDiagram )
{
    bool bRet = false;

    Reference< XCoordinateSystem > xCooSys = AxisHelper::getCoordinateSystemByIndex( xDiagram, nCooSysIndex );
    if(!xCooSys.is())
        return bRet;

    Reference< XAxis > xAxis( AxisHelper::getAxis( nDimensionIndex, MAIN_AXIS_INDEX, xCooSys ) );
    if(!xAxis.is())
        return bRet;

    if( bMainGrid )
        bRet = AxisHelper::isGridVisible( xAxis->getGridProperties() );
    else
    {
        Sequence< Reference< beans::XPropertySet > > aSubGrids( xAxis->getSubGridProperties() );
        if( aSubGrids.hasElements() )
            bRet = AxisHelper::isGridVisible( aSubGrids[0] );
    }

    return bRet;
}

Reference< XCoordinateSystem > AxisHelper::getCoordinateSystemByIndex(
    const Reference< XDiagram >& xDiagram, sal_Int32 nIndex )
{
    Reference< XCoordinateSystemContainer > xCooSysContainer( xDiagram, uno::UNO_QUERY );
    if(!xCooSysContainer.is())
        return nullptr;
    Sequence< Reference< XCoordinateSystem > > aCooSysList = xCooSysContainer->getCoordinateSystems();
    if(0<=nIndex && nIndex<aCooSysList.getLength())
        return aCooSysList[nIndex];
    return nullptr;
}

Reference< XAxis > AxisHelper::getAxis( sal_Int32 nDimensionIndex, bool bMainAxis
            , const Reference< XDiagram >& xDiagram )
{
    Reference< XAxis > xRet;
    try
    {
        Reference< XCoordinateSystem > xCooSys = AxisHelper::getCoordinateSystemByIndex( xDiagram, 0 );
        xRet.set( AxisHelper::getAxis( nDimensionIndex, bMainAxis ? 0 : 1, xCooSys ) );
    }
    catch( const uno::Exception & )
    {
    }
    return xRet;
}

Reference< XAxis > AxisHelper::getAxis( sal_Int32 nDimensionIndex, sal_Int32 nAxisIndex
            , const Reference< XCoordinateSystem >& xCooSys )
{
    Reference< XAxis > xRet;
    if(!xCooSys.is())
        return xRet;

    if(nDimensionIndex >= xCooSys->getDimension())
        return xRet;

    if(nAxisIndex > xCooSys->getMaximumAxisIndexByDimension(nDimensionIndex))
        return xRet;

    assert(nAxisIndex >= 0);
    assert(nDimensionIndex >= 0);
    xRet.set( xCooSys->getAxisByDimension( nDimensionIndex, nAxisIndex ) );
    return xRet;
}

Reference< XAxis > AxisHelper::getCrossingMainAxis( const Reference< XAxis >& xAxis
            , const Reference< XCoordinateSystem >& xCooSys )
{
    sal_Int32 nDimensionIndex = 0;
    sal_Int32 nAxisIndex = 0;
    AxisHelper::getIndicesForAxis( xAxis, xCooSys, nDimensionIndex, nAxisIndex );
    if( nDimensionIndex==2 )
    {
        nDimensionIndex=1;
        bool bSwapXY = false;
        Reference< beans::XPropertySet > xCooSysProp( xCooSys, uno::UNO_QUERY );
        if( xCooSysProp.is() && (xCooSysProp->getPropertyValue( "SwapXAndYAxis" ) >>= bSwapXY) && bSwapXY )
            nDimensionIndex=0;
    }
    else if( nDimensionIndex==1 )
        nDimensionIndex=0;
    else
        nDimensionIndex=1;
    return AxisHelper::getAxis( nDimensionIndex, 0, xCooSys );
}

Reference< XAxis > AxisHelper::getParallelAxis( const Reference< XAxis >& xAxis
            , const Reference< XDiagram >& xDiagram )
{
    try
    {
        sal_Int32 nCooSysIndex=-1;
        sal_Int32 nDimensionIndex=-1;
        sal_Int32 nAxisIndex=-1;
        if( getIndicesForAxis( xAxis, xDiagram, nCooSysIndex, nDimensionIndex, nAxisIndex ) )
        {
            sal_Int32 nParallelAxisIndex = (nAxisIndex==1) ?0 :1;
            return getAxis( nDimensionIndex, nParallelAxisIndex, getCoordinateSystemByIndex( xDiagram, nCooSysIndex ) );
        }
    }
    catch( const uno::RuntimeException& )
    {
    }
    return nullptr;
}

bool AxisHelper::isAxisShown( sal_Int32 nDimensionIndex, bool bMainAxis
            , const Reference< XDiagram >& xDiagram )
{
    return AxisHelper::isAxisVisible( AxisHelper::getAxis( nDimensionIndex, bMainAxis, xDiagram ) );
}

bool AxisHelper::isAxisVisible( const Reference< XAxis >& xAxis )
{
    bool bRet = false;

    Reference< beans::XPropertySet > xProps( xAxis, uno::UNO_QUERY );
    if( xProps.is() )
    {
        xProps->getPropertyValue( "Show" ) >>= bRet;
        bRet = bRet && ( LinePropertiesHelper::IsLineVisible( xProps )
            || areAxisLabelsVisible( xProps ) );
    }

    return bRet;
}

bool AxisHelper::areAxisLabelsVisible( const Reference< beans::XPropertySet >& xAxisProperties )
{
    bool bRet = false;
    if( xAxisProperties.is() )
    {
        xAxisProperties->getPropertyValue( "DisplayLabels" ) >>= bRet;
    }
    return bRet;
}

bool AxisHelper::isGridVisible( const Reference< beans::XPropertySet >& xGridproperties )
{
    bool bRet = false;

    if( xGridproperties.is() )
    {
        xGridproperties->getPropertyValue( "Show" ) >>= bRet;
        bRet = bRet && LinePropertiesHelper::IsLineVisible( xGridproperties );
    }

    return bRet;
}

Reference< beans::XPropertySet > AxisHelper::getGridProperties(
            const Reference< XCoordinateSystem >& xCooSys
        , sal_Int32 nDimensionIndex, sal_Int32 nAxisIndex, sal_Int32 nSubGridIndex )
{
    Reference< beans::XPropertySet > xRet;

    Reference< XAxis > xAxis( AxisHelper::getAxis( nDimensionIndex, nAxisIndex, xCooSys ) );
    if( xAxis.is() )
    {
        if( nSubGridIndex<0 )
            xRet.set( xAxis->getGridProperties() );
        else
        {
            Sequence< Reference< beans::XPropertySet > > aSubGrids( xAxis->getSubGridProperties() );
            if (nSubGridIndex < aSubGrids.getLength())
                xRet.set( aSubGrids[nSubGridIndex] );
        }
    }

    return xRet;
}

sal_Int32 AxisHelper::getDimensionIndexOfAxis(
              const Reference< XAxis >& xAxis
            , const Reference< XDiagram >& xDiagram )
{
    sal_Int32 nDimensionIndex = -1;
    sal_Int32 nCooSysIndex = -1;
    sal_Int32 nAxisIndex = -1;
    AxisHelper::getIndicesForAxis( xAxis, xDiagram, nCooSysIndex , nDimensionIndex, nAxisIndex );
    return nDimensionIndex;
}

bool AxisHelper::getIndicesForAxis(
              const Reference< XAxis >& xAxis
            , const Reference< XCoordinateSystem >& xCooSys
            , sal_Int32& rOutDimensionIndex, sal_Int32& rOutAxisIndex )
{
    //returns true if indices are found

    rOutDimensionIndex = -1;
    rOutAxisIndex = -1;

    if( !xCooSys || !xAxis )
        return false;

    Reference< XAxis > xCurrentAxis;
    sal_Int32 nDimensionCount( xCooSys->getDimension() );
    for( sal_Int32 nDimensionIndex = 0; nDimensionIndex < nDimensionCount; nDimensionIndex++ )
    {
        sal_Int32 nMaxAxisIndex = xCooSys->getMaximumAxisIndexByDimension(nDimensionIndex);
        for( sal_Int32 nAxisIndex = 0; nAxisIndex <= nMaxAxisIndex; nAxisIndex++ )
        {
             xCurrentAxis = xCooSys->getAxisByDimension(nDimensionIndex,nAxisIndex);
             if( xCurrentAxis == xAxis )
             {
                 rOutDimensionIndex = nDimensionIndex;
                 rOutAxisIndex = nAxisIndex;
                 return true;
             }
        }
    }
    return false;
}

bool AxisHelper::getIndicesForAxis( const Reference< XAxis >& xAxis, const Reference< XDiagram >& xDiagram
            , sal_Int32& rOutCooSysIndex, sal_Int32& rOutDimensionIndex, sal_Int32& rOutAxisIndex )
{
    //returns true if indices are found

    rOutCooSysIndex = -1;
    rOutDimensionIndex = -1;
    rOutAxisIndex = -1;

    Reference< XCoordinateSystemContainer > xCooSysContainer( xDiagram, uno::UNO_QUERY );
    if(xCooSysContainer.is())
    {
        Sequence< Reference< XCoordinateSystem > > aCooSysList = xCooSysContainer->getCoordinateSystems();
        for( sal_Int32 nC=0; nC<aCooSysList.getLength(); ++nC )
        {
            if( AxisHelper::getIndicesForAxis( xAxis, aCooSysList[nC], rOutDimensionIndex, rOutAxisIndex ) )
            {
                rOutCooSysIndex = nC;
                return true;
            }
        }
    }

    return false;
}

std::vector< Reference< XAxis > > AxisHelper::getAllAxesOfCoordinateSystem(
      const Reference< XCoordinateSystem >& xCooSys
    , bool bOnlyVisible /* = false */ )
{
    std::vector< Reference< XAxis > > aAxisVector;

    if(xCooSys.is())
    {
        sal_Int32 nMaxDimensionIndex = xCooSys->getDimension() -1;
        if( nMaxDimensionIndex>=0 )
        {
            sal_Int32 nDimensionIndex = 0;
            for(; nDimensionIndex<=nMaxDimensionIndex; ++nDimensionIndex)
            {
                const sal_Int32 nMaximumAxisIndex = xCooSys->getMaximumAxisIndexByDimension(nDimensionIndex);
                for(sal_Int32 nAxisIndex=0; nAxisIndex<=nMaximumAxisIndex; ++nAxisIndex)
                {
                    try
                    {
                        Reference< XAxis > xAxis( xCooSys->getAxisByDimension( nDimensionIndex, nAxisIndex ) );
                        if( xAxis.is() )
                        {
                            bool bAddAxis = true;
                            if( bOnlyVisible )
                            {
                                Reference< beans::XPropertySet > xAxisProp( xAxis, uno::UNO_QUERY );
                                if( !xAxisProp.is() ||
                                    !(xAxisProp->getPropertyValue( "Show") >>= bAddAxis) )
                                    bAddAxis = false;
                            }
                            if( bAddAxis )
                                aAxisVector.push_back( xAxis );
                        }
                    }
                    catch( const uno::Exception & )
                    {
                        DBG_UNHANDLED_EXCEPTION("chart2");
                    }
                }
            }
        }
    }

    return aAxisVector;
}

Sequence< Reference< XAxis > > AxisHelper::getAllAxesOfDiagram(
      const Reference< XDiagram >& xDiagram
    , bool bOnlyVisible )
{
    std::vector< Reference< XAxis > > aAxisVector;

    Reference< XCoordinateSystemContainer > xCooSysContainer( xDiagram, uno::UNO_QUERY );
    if(xCooSysContainer.is())
    {
        const Sequence< Reference< XCoordinateSystem > > aCooSysList = xCooSysContainer->getCoordinateSystems();
        for( Reference< XCoordinateSystem > const & coords : aCooSysList )
        {
            std::vector< Reference< XAxis > > aAxesPerCooSys( AxisHelper::getAllAxesOfCoordinateSystem( coords, bOnlyVisible ) );
            aAxisVector.insert( aAxisVector.end(), aAxesPerCooSys.begin(), aAxesPerCooSys.end() );
        }
    }

    return comphelper::containerToSequence( aAxisVector );
}

Sequence< Reference< beans::XPropertySet > > AxisHelper::getAllGrids( const Reference< XDiagram >& xDiagram )
{
    const Sequence< Reference< XAxis > > aAllAxes( AxisHelper::getAllAxesOfDiagram( xDiagram ) );
    std::vector< Reference< beans::XPropertySet > > aGridVector;

    for( Reference< XAxis > const & xAxis : aAllAxes )
    {
        if(!xAxis.is())
            continue;
        Reference< beans::XPropertySet > xGridProperties( xAxis->getGridProperties() );
        if( xGridProperties.is() )
            aGridVector.push_back( xGridProperties );

        const Sequence< Reference< beans::XPropertySet > > aSubGrids( xAxis->getSubGridProperties() );
        for( Reference< beans::XPropertySet > const & xSubGrid : aSubGrids )
        {
            if( xSubGrid.is() )
                aGridVector.push_back( xSubGrid );
        }
    }

    return comphelper::containerToSequence( aGridVector );
}

void AxisHelper::getAxisOrGridPossibilities( Sequence< sal_Bool >& rPossibilityList
        , const Reference< XDiagram>& xDiagram, bool bAxis )
{
    rPossibilityList.realloc(6);
    sal_Bool* pPossibilityList = rPossibilityList.getArray();

    sal_Int32 nDimensionCount = DiagramHelper::getDimension( xDiagram );

    //set possibilities:
    sal_Int32 nIndex=0;
    Reference< XChartType > xChartType = DiagramHelper::getChartTypeByIndex( xDiagram, 0 );
    for(nIndex=0;nIndex<3;nIndex++)
        pPossibilityList[nIndex]=ChartTypeHelper::isSupportingMainAxis(xChartType,nDimensionCount,nIndex);
    for(nIndex=3;nIndex<6;nIndex++)
        if( bAxis )
            pPossibilityList[nIndex]=ChartTypeHelper::isSupportingSecondaryAxis(xChartType,nDimensionCount);
        else
            pPossibilityList[nIndex] = rPossibilityList[nIndex-3];
}

bool AxisHelper::isSecondaryYAxisNeeded( const Reference< XCoordinateSystem >& xCooSys )
{
    Reference< chart2::XChartTypeContainer > xCTCnt( xCooSys, uno::UNO_QUERY );
    if( !xCTCnt.is() )
        return false;

    const Sequence< Reference< chart2::XChartType > > aChartTypes( xCTCnt->getChartTypes() );
    for( Reference< chart2::XChartType > const & chartType : aChartTypes )
    {
        Reference< XDataSeriesContainer > xSeriesContainer( chartType, uno::UNO_QUERY );
        if( !xSeriesContainer.is() )
                continue;

        Sequence< Reference< XDataSeries > > aSeriesList( xSeriesContainer->getDataSeries() );
        for( sal_Int32 nS = aSeriesList.getLength(); nS-- ; )
        {
            Reference< beans::XPropertySet > xProp( aSeriesList[nS], uno::UNO_QUERY );
            if(xProp.is())
            {
                sal_Int32 nAttachedAxisIndex = 0;
                if( ( xProp->getPropertyValue( "AttachedAxisIndex" ) >>= nAttachedAxisIndex ) && nAttachedAxisIndex>0 )
                    return true;
            }
        }
    }
    return false;
}

bool AxisHelper::shouldAxisBeDisplayed( const Reference< XAxis >& xAxis
                                       , const Reference< XCoordinateSystem >& xCooSys )
{
    bool bRet = false;

    if( xAxis.is() && xCooSys.is() )
    {
        sal_Int32 nDimensionIndex=-1;
        sal_Int32 nAxisIndex=-1;
        if( AxisHelper::getIndicesForAxis( xAxis, xCooSys, nDimensionIndex, nAxisIndex ) )
        {
            sal_Int32 nDimensionCount = xCooSys->getDimension();
            Reference< XChartType > xChartType( AxisHelper::getChartTypeByIndex( xCooSys, 0 ) );

            bool bMainAxis = (nAxisIndex==MAIN_AXIS_INDEX);
            if( bMainAxis )
                bRet = ChartTypeHelper::isSupportingMainAxis(xChartType,nDimensionCount,nDimensionIndex);
            else
                bRet = ChartTypeHelper::isSupportingSecondaryAxis(xChartType,nDimensionCount);
        }
    }

    return bRet;
}

void AxisHelper::getAxisOrGridExistence( Sequence< sal_Bool >& rExistenceList
        , const Reference< XDiagram>& xDiagram, bool bAxis )
{
    rExistenceList.realloc(6);
    sal_Bool* pExistenceList = rExistenceList.getArray();

    if(bAxis)
    {
        sal_Int32 nN;
        for(nN=0;nN<3;nN++)
            pExistenceList[nN] = AxisHelper::isAxisShown( nN, true, xDiagram );
        for(nN=3;nN<6;nN++)
            pExistenceList[nN] = AxisHelper::isAxisShown( nN%3, false, xDiagram );
    }
    else
    {
        sal_Int32 nN;

        for(nN=0;nN<3;nN++)
            pExistenceList[nN] = AxisHelper::isGridShown( nN, 0, true, xDiagram );
        for(nN=3;nN<6;nN++)
            pExistenceList[nN] = AxisHelper::isGridShown( nN%3, 0, false, xDiagram );
    }
}

bool AxisHelper::changeVisibilityOfAxes( const Reference< XDiagram >& xDiagram
                        , const Sequence< sal_Bool >& rOldExistenceList
                        , const Sequence< sal_Bool >& rNewExistenceList
                        , const Reference< uno::XComponentContext >& xContext
                        , ReferenceSizeProvider * pRefSizeProvider )
{
    bool bChanged = false;
    for(sal_Int32 nN=0;nN<6;nN++)
    {
        if(rOldExistenceList[nN]!=rNewExistenceList[nN])
        {
            bChanged = true;
            if(rNewExistenceList[nN])
            {
                AxisHelper::showAxis( nN%3, nN<3, xDiagram, xContext, pRefSizeProvider );
            }
            else
                AxisHelper::hideAxis( nN%3, nN<3, xDiagram );
        }
    }
    return bChanged;
}

bool AxisHelper::changeVisibilityOfGrids( const Reference< XDiagram >& xDiagram
                        , const Sequence< sal_Bool >& rOldExistenceList
                        , const Sequence< sal_Bool >& rNewExistenceList )
{
    bool bChanged = false;
    for(sal_Int32 nN=0;nN<6;nN++)
    {
        if(rOldExistenceList[nN]!=rNewExistenceList[nN])
        {
            bChanged = true;
            if(rNewExistenceList[nN])
                AxisHelper::showGrid( nN%3, 0, nN<3, xDiagram );
            else
                AxisHelper::hideGrid( nN%3, 0, nN<3, xDiagram );
        }
    }
    return bChanged;
}

Reference< XCoordinateSystem > AxisHelper::getCoordinateSystemOfAxis(
              const Reference< XAxis >& xAxis
            , const Reference< XDiagram >& xDiagram )
{
    Reference< XCoordinateSystem > xRet;

    Reference< XCoordinateSystemContainer > xCooSysContainer( xDiagram, uno::UNO_QUERY );
    if( xCooSysContainer.is() )
    {
        const Sequence< Reference< XCoordinateSystem > > aCooSysList( xCooSysContainer->getCoordinateSystems() );
        for( Reference< XCoordinateSystem > const & xCooSys : aCooSysList )
        {
            std::vector< Reference< XAxis > > aAllAxis( AxisHelper::getAllAxesOfCoordinateSystem( xCooSys ) );

            std::vector< Reference< XAxis > >::iterator aFound =
                  std::find( aAllAxis.begin(), aAllAxis.end(), xAxis );
            if( aFound != aAllAxis.end())
            {
                xRet.set( xCooSys );
                break;
            }
        }
    }
    return xRet;
}

Reference< XChartType > AxisHelper::getChartTypeByIndex( const Reference< XCoordinateSystem >& xCooSys, sal_Int32 nIndex )
{
    Reference< XChartType > xChartType;

    Reference< XChartTypeContainer > xChartTypeContainer( xCooSys, uno::UNO_QUERY );
    if( xChartTypeContainer.is() )
    {
        Sequence< Reference< XChartType > > aChartTypeList( xChartTypeContainer->getChartTypes() );
        if( nIndex >= 0 && nIndex < aChartTypeList.getLength() )
            xChartType.set( aChartTypeList[nIndex] );
    }

    return xChartType;
}

void AxisHelper::setRTLAxisLayout( const Reference< XCoordinateSystem >& xCooSys )
{
    if( !xCooSys.is() )
        return;

    bool bCartesian = xCooSys->getViewServiceName() == CHART2_COOSYSTEM_CARTESIAN_VIEW_SERVICE_NAME;
    if( !bCartesian )
        return;

    bool bVertical = false;
    Reference< beans::XPropertySet > xCooSysProp( xCooSys, uno::UNO_QUERY );
    if( xCooSysProp.is() )
        xCooSysProp->getPropertyValue( "SwapXAndYAxis" ) >>= bVertical;

    sal_Int32 nHorizontalAxisDimension = bVertical ? 1 : 0;
    sal_Int32 nVerticalAxisDimension = bVertical ? 0 : 1;

    try
    {
        //reverse direction for horizontal main axis
        Reference< chart2::XAxis > xHorizontalMainAxis( AxisHelper::getAxis( nHorizontalAxisDimension, MAIN_AXIS_INDEX, xCooSys ) );
        if( xHorizontalMainAxis.is() )
        {
            chart2::ScaleData aScale = xHorizontalMainAxis->getScaleData();
            aScale.Orientation = chart2::AxisOrientation_REVERSE;
            xHorizontalMainAxis->setScaleData(aScale);
        }

        //mathematical direction for vertical main axis
        Reference< chart2::XAxis > xVerticalMainAxis( AxisHelper::getAxis( nVerticalAxisDimension, MAIN_AXIS_INDEX, xCooSys ) );
        if( xVerticalMainAxis.is() )
        {
            chart2::ScaleData aScale = xVerticalMainAxis->getScaleData();
            aScale.Orientation = chart2::AxisOrientation_MATHEMATICAL;
            xVerticalMainAxis->setScaleData(aScale);
        }
    }
    catch( const uno::Exception & )
    {
        DBG_UNHANDLED_EXCEPTION("chart2" );
    }

    try
    {
        //reverse direction for horizontal secondary axis
        Reference< chart2::XAxis > xHorizontalSecondaryAxis( AxisHelper::getAxis( nHorizontalAxisDimension, SECONDARY_AXIS_INDEX, xCooSys ) );
        if( xHorizontalSecondaryAxis.is() )
        {
            chart2::ScaleData aScale = xHorizontalSecondaryAxis->getScaleData();
            aScale.Orientation = chart2::AxisOrientation_REVERSE;
            xHorizontalSecondaryAxis->setScaleData(aScale);
        }

        //mathematical direction for vertical secondary axis
        Reference< chart2::XAxis > xVerticalSecondaryAxis( AxisHelper::getAxis( nVerticalAxisDimension, SECONDARY_AXIS_INDEX, xCooSys ) );
        if( xVerticalSecondaryAxis.is() )
        {
            chart2::ScaleData aScale = xVerticalSecondaryAxis->getScaleData();
            aScale.Orientation = chart2::AxisOrientation_MATHEMATICAL;
            xVerticalSecondaryAxis->setScaleData(aScale);
        }
    }
    catch( const uno::Exception & )
    {
        DBG_UNHANDLED_EXCEPTION("chart2");
    }
}

Reference< XChartType > AxisHelper::getFirstChartTypeWithSeriesAttachedToAxisIndex( const Reference< chart2::XDiagram >& xDiagram, const sal_Int32 nAttachedAxisIndex )
{
    Reference< XChartType > xChartType;
    std::vector< Reference< XDataSeries > > aSeriesVector( DiagramHelper::getDataSeriesFromDiagram( xDiagram ) );
    for (auto const& series : aSeriesVector)
    {
        sal_Int32 nCurrentIndex = DataSeriesHelper::getAttachedAxisIndex(series);
        if( nAttachedAxisIndex == nCurrentIndex )
        {
            xChartType = DiagramHelper::getChartTypeOfSeries(xDiagram, series);
            if(xChartType.is())
                break;
        }
    }
    return xChartType;
}

bool AxisHelper::isAxisPositioningEnabled()
{
    const SvtSaveOptions::ODFSaneDefaultVersion nCurrentVersion(GetODFSaneDefaultVersion());
    return nCurrentVersion >= SvtSaveOptions::ODFSVER_012;
}

} //namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
