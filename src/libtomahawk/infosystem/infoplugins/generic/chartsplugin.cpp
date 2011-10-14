/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Hugo Lindström <hugolm84@gmail.com>
 *   Copyright 2011, Leo Franchi <lfranchi@kde.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "chartsplugin.h"

#include <QDir>
#include <QSettings>
#include <QCryptographicHash>
#include <QNetworkConfiguration>
#include <QNetworkReply>
#include <QDomElement>

#include "album.h"
#include "typedefs.h"
#include "audio/audioengine.h"
#include "tomahawksettings.h"
#include "utils/tomahawkutils.h"
#include "utils/logger.h"

#define CHART_URL "http://charts.tomahawk-player.org:10080/"
//#define CHART_URL "http://localhost:8080/"
#include <qjson/parser.h>
#include <qjson/serializer.h>

using namespace Tomahawk::InfoSystem;


ChartsPlugin::ChartsPlugin()
    : InfoPlugin()
    , m_chartsFetchJobs( 0 )
{


    /// Add resources here
    m_chartResources << "billboard" << "itunes";
    m_supportedGetTypes <<  InfoChart << InfoChartCapabilities;

}


ChartsPlugin::~ChartsPlugin()
{
    qDebug() << Q_FUNC_INFO;
}


void
ChartsPlugin::namChangedSlot( QNetworkAccessManager *nam )
{
    tDebug() << "ChartsPlugin: namChangedSLot";

    qDebug() << Q_FUNC_INFO;
    if( !nam )
        return;

    m_nam = QWeakPointer< QNetworkAccessManager >( nam );

    /// Then get each chart from resource
    /// We need to fetch them before they are asked for

    if( !m_chartResources.isEmpty() && m_nam ){

        tDebug() << "ChartsPlugin: InfoChart fetching possible resources";
        foreach ( QVariant resource, m_chartResources )
        {
            QUrl url = QUrl( QString( CHART_URL "source/%1" ).arg(resource.toString() ) );
            QNetworkReply* reply = m_nam.data()->get( QNetworkRequest( url ) );
            tDebug() << "fetching:" << url;
            connect( reply, SIGNAL( finished() ), SLOT( chartTypes() ) );

            m_chartsFetchJobs++;
        }

    }
}


void
ChartsPlugin::dataError( uint requestId, Tomahawk::InfoSystem::InfoRequestData requestData )
{
    emit info( requestId, requestData, QVariant() );
    return;
}


void
ChartsPlugin::getInfo( uint requestId, Tomahawk::InfoSystem::InfoRequestData requestData )
{
    qDebug() << Q_FUNC_INFO << requestData.caller;
    qDebug() << Q_FUNC_INFO << requestData.customData;

    InfoCriteriaHash hash = requestData.input.value< Tomahawk::InfoSystem::InfoCriteriaHash >();
    bool foundSource;

    switch ( requestData.type )
    {

        case InfoChart:
            /// We need something to check if the request is actually ment to go to this plugin
            if ( !hash.contains( "chart_source" ) )
            {
                dataError( requestId, requestData );
                break;
            }
            else
            {
                foreach( QVariant resource, m_chartResources )
                {
                    if( resource.toString() == hash["chart_source"] )
                    {
                        foundSource = true;
                    }
                }

                if( !foundSource )
                {
                    dataError( requestId, requestData );
                    break;
                }

            }
            fetchChart( requestId, requestData );
            break;

        case InfoChartCapabilities:
            fetchChartCapabilities( requestId, requestData );
            break;
        default:
            dataError( requestId, requestData );
    }
}


void
ChartsPlugin::pushInfo( const QString caller, const Tomahawk::InfoSystem::InfoType type, const QVariant input )
{
    Q_UNUSED( caller )
    Q_UNUSED( type)
    Q_UNUSED( input )
}


void
ChartsPlugin::fetchChart( uint requestId, Tomahawk::InfoSystem::InfoRequestData requestData )
{

    if ( !requestData.input.canConvert< Tomahawk::InfoSystem::InfoCriteriaHash >() )
    {
        dataError( requestId, requestData );
        return;
    }

    InfoCriteriaHash hash = requestData.input.value< Tomahawk::InfoSystem::InfoCriteriaHash >();
    Tomahawk::InfoSystem::InfoCriteriaHash criteria;

    /// Each request needs to contain both a id and source
    if ( !hash.contains( "chart_id" ) && !hash.contains( "chart_source" ) )
    {
        dataError( requestId, requestData );
        return;

    }
    /// Set the criterias for current chart
    criteria["chart_id"] = hash["chart_id"];
    criteria["chart_source"] = hash["chart_source"];

    emit getCachedInfo( requestId, criteria, 0, requestData );
}

void
ChartsPlugin::fetchChartCapabilities( uint requestId, Tomahawk::InfoSystem::InfoRequestData requestData )
{
    if ( !requestData.input.canConvert< Tomahawk::InfoSystem::InfoCriteriaHash >() )
    {
        dataError( requestId, requestData );
        return;
    }

    Tomahawk::InfoSystem::InfoCriteriaHash criteria;
    emit getCachedInfo( requestId, criteria, 0, requestData );
}

void
ChartsPlugin::notInCacheSlot( uint requestId, QHash<QString, QString> criteria, Tomahawk::InfoSystem::InfoRequestData requestData )
{
    if ( !m_nam.data() )
    {
        tLog() << "Have a null QNAM, uh oh";
        emit info( requestId, requestData, QVariant() );
        return;
    }


    switch ( requestData.type )
    {
        case InfoChart:
        {

            /// Fetch the chart, we need source and id
            QUrl url = QUrl( QString( CHART_URL "source/%1/chart/%2" ).arg( criteria["chart_source"] ).arg( criteria["chart_id"] ) );
            qDebug() << Q_FUNC_INFO << "Getting chart url" << url;

            QNetworkReply* reply = m_nam.data()->get( QNetworkRequest( url ) );
            reply->setProperty( "requestId", requestId );
            reply->setProperty( "requestData", QVariant::fromValue< Tomahawk::InfoSystem::InfoRequestData >( requestData ) );

            connect( reply, SIGNAL( finished() ), SLOT( chartReturned() ) );
            return;

        }

        case InfoChartCapabilities:
        {
            if ( m_chartsFetchJobs > 0 )
            {
                qDebug() << Q_FUNC_INFO << "InfoChartCapabilities still fetching!";
                m_cachedRequests.append( QPair< uint, InfoRequestData >( requestId, requestData ) );
                return;
            }

            emit info(
                requestId,
                requestData,
                m_allChartsMap
            );
            return;
        }

        default:
        {
            tLog() << Q_FUNC_INFO << "Couldn't figure out what to do with this type of request after cache miss";
            emit info( requestId, requestData, QVariant() );
            return;
        }
    }
}


void
ChartsPlugin::chartTypes()
{
    /// Get possible chart type for specificChartsPlugin: InfoChart types returned chart source
    tDebug() << "Got chart type result";
    QNetworkReply* reply = qobject_cast<QNetworkReply*>( sender() );

    if ( reply->error() == QNetworkReply::NoError )
    {
        QJson::Parser p;
        bool ok;
        const QVariantMap res = p.parse( reply, &ok ).toMap();
        const QVariantMap chartObjs = res.value( "charts" ).toMap();

        if ( !ok )
        {
            tLog() << "Failed to parse resources" << p.errorString() << "On line" << p.errorLine();

            return;
        }

        /// Got types, append!
        const QString source = res.value( "source" ).toString();

        // We'll populate charts with the data from the server
        QVariantMap charts;
        QString chartName;
        if ( source == "itunes" )
        {
            // Itunes has geographic-area based charts. So we build a breadcrumb of
            // ITunes - Country - Albums - Top Chart Type
            //                  - Tracks - Top Chart Type
            QHash< QString, QVariantMap > countries;
            foreach( const QVariant& chartObj, chartObjs.values() )
            {
                const QVariantMap chart = chartObj.toMap();
                const QString id = chart.value( "id" ).toString();
                const QString country = tr( "Country: %1" ).arg( chart.value( "geo" ).toString() );
                QString name = chart.value( "name" ).toString();
                const QString type = chart.value( "type" ).toString();

                if ( name.startsWith( "iTunes Store:" ) ) // truncate
                    name = name.mid( 13 );

                const Chart c( id, name, "album" );
                QList<Chart> countryTypeData = countries[ country ][ type ].value<QList<Chart> >();
                countryTypeData.append( c );

                countries[ country ].insert( type, QVariant::fromValue<QList<Chart> >( countryTypeData ) );
            }

            foreach( const QString& c, countries.keys() )
            {
                charts[ c ] = countries[ c ];
//                 qDebug() << "Country has types:" << countries[ c ];
            }
            chartName = "iTunes";
        } else
        {
            // We'll just build:
            // [Source] - Album - Chart Type
            // [Source] - Track - Chart Type
            QList< Chart > albumCharts;
            QList< Chart > trackCharts;
            foreach( const QVariant& chartObj, chartObjs.values() )
            {
                const QVariantMap chart = chartObj.toMap();
                const QString type = chart.value( "type" ).toString();
                const QString id = chart.value( "id" ).toString();
                const QString name = chart.value( "name" ).toString();
                if ( type == "Album" )
                    albumCharts.append( Chart(  id, name, "album" ) );
                else if ( type == "Track" )
                    trackCharts.append( Chart( id, name, "tracks" ) );
            }
            charts.insert( tr( "Albums" ), QVariant::fromValue< QList<Chart> >( albumCharts ) );
            charts.insert( tr( "Tracks" ), QVariant::fromValue< QList<Chart> >( trackCharts ) );

            /// @note For displaying purposes, upper the first letter
            /// @note Remeber to lower it when fetching this!
            chartName = source;
            chartName[0] = chartName[0].toUpper();
        }

        /// Add the possible charts and its types to breadcrumb
//         qDebug() << "ADDING CHART TYPE TO CHARTS:" << chartName;
        m_allChartsMap.insert( chartName , QVariant::fromValue<QVariantMap>( charts ) );

    }
    else
    {
        tLog() << "Error fetching charts:" << reply->errorString();
    }

    m_chartsFetchJobs--;
    if ( !m_cachedRequests.isEmpty() && m_chartsFetchJobs == 0 )
    {
        QPair< uint, InfoRequestData > request;
        foreach ( request, m_cachedRequests )
        {
            emit info( request.first, request.second, m_allChartsMap );
        }
        m_cachedRequests.clear();
    }

}

void
ChartsPlugin::chartReturned()
{

    /// Chart request returned something! Woho
    QNetworkReply* reply = qobject_cast<QNetworkReply*>( sender() );
    QVariantMap returnedData;

    if ( reply->error() == QNetworkReply::NoError )
    {
        QJson::Parser p;
        bool ok;
        QVariantMap res = p.parse( reply, &ok ).toMap();

        if ( !ok )
        {
            tLog() << "Failed to parse json from chart lookup:" << p.errorString() << "On line" << p.errorLine();
            return;
        }

        /// SO we have a result, parse it!
        QVariantList chartResponse = res.value( "list" ).toList();
        QList<ArtistTrackPair> top_tracks;
        QList<ArtistAlbumPair> top_albums;

        /// Deside what type, we need to handle it differently
        /// @todo: We allready know the type, append it to breadcrumb hash

        if( res.value( "type" ).toString() == "Album" )
            setChartType( Album );
        else if( res.value( "type" ).toString() == "Track" )
            setChartType( Track );
        else
            setChartType( None );


        qDebug() << "Got chart returned!" << res;
        foreach ( QVariant chartR, chartResponse )
        {
            QString title, artist, album;
            QVariantMap chartMap = chartR.toMap();

            if ( !chartMap.isEmpty() )
            {

                title = chartMap.value( "track" ).toString();
                album = chartMap.value( "album" ).toString();
                artist = chartMap.value( "artist" ).toString();
                /// Maybe we can use rank later on, to display something nice
                /// rank = chartMap.value( "rank" ).toString();

                if ( chartType() == Album )
                {
                     /** HACK, billboard chart returns wrong typename **/
                    if ( res.value( "source" ).toString() == "billboard" )
                        album = chartMap.value( "track" ).toString();

                    if ( album.isEmpty() && artist.isEmpty() ) // don't have enough...
                    {
                        tLog() << "Didn't get an artist and album name from chart, not enough to build a query on. Aborting" << title << album << artist;

                    }
                    else
                    {
                        qDebug() << Q_FUNC_INFO << album << artist;
                        ArtistAlbumPair pair;
                        pair.artist = artist;
                        pair.album = album;
                        top_albums << pair;

                    }
                }

                else if ( chartType() == Track )
                {

                    if ( title.isEmpty() && artist.isEmpty() ) // don't have enough...
                    {
                        tLog() << "Didn't get an artist and track name from charts, not enough to build a query on. Aborting" << title << artist << album;

                    }
                    else
                    {

                        ArtistTrackPair pair;
                        pair.artist = artist;
                        pair.track = title;
                        top_tracks << pair;

                    }
                }
            }
        }

        if( chartType() == Track )
        {
            tDebug() << "ChartsPlugin:" << "\tgot " << top_tracks.size() << " tracks";
            returnedData["tracks"] = QVariant::fromValue( top_tracks );
            returnedData["type"] = "tracks";
        }

        if( chartType() == Album )
        {
            tDebug() << "ChartsPlugin:" << "\tgot " << top_albums.size() << " albums";
            returnedData["albums"] = QVariant::fromValue( top_albums );
            returnedData["type"] = "albums";
        }

        Tomahawk::InfoSystem::InfoRequestData requestData = reply->property( "requestData" ).value< Tomahawk::InfoSystem::InfoRequestData >();


        emit info(
            reply->property( "requestId" ).toUInt(),
            requestData,
            returnedData
        );
        // TODO update cache
    }
    else
        qDebug() << "Network error in fetching chart:" << reply->url().toString();

}
