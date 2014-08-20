/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2014, Christian Muehlhaeuser <muesli@tomahawk-player.org>
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

#include "TrackDetailView.h"

#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "Album.h"
#include "Track.h"
#include "audio/AudioEngine.h"
#include "widgets/PlayableCover.h"
#include "widgets/QueryLabel.h"
#include "widgets/ClickableLabel.h"
#include "PlaylistInterface.h"
#include "utils/ImageRegistry.h"
#include "utils/TomahawkUtilsGui.h"
#include "utils/Closure.h"
#include "utils/Logger.h"

using namespace Tomahawk;

TrackDetailView::TrackDetailView( QWidget* parent )
    : QWidget( parent )
{
    setFixedWidth( 230 );
    setContentsMargins( 0, 0, 0, 0 );

    QPalette pal( palette() );
    pal.setColor( QPalette::Background, Qt::white );
    setPalette( pal );
    setAutoFillBackground( true );

    m_playableCover = new PlayableCover( this );
    m_playableCover->setShowText( false );
    m_playableCover->setShowControls( false );
    m_playableCover->setFixedWidth( 230 );
    m_playableCover->setFixedHeight( 230 );

    QFont f = font();
    m_nameLabel = new QueryLabel( this );
    f.setPointSize( 13 );
    m_nameLabel->setFont( f );

    m_dateLabel = new QLabel( this );
    f.setPointSize( 11 );
    m_dateLabel->setFont( f );
    m_dateLabel->setStyleSheet( "QLabel { color: rgba( 0, 0, 0, 70% ) }" );

    m_lovedIcon = new QLabel( this );
    m_lovedIcon->setFixedWidth( 12 );
    m_lovedLabel = new QLabel( this );
    f.setWeight( QFont::DemiBold );
    f.setPointSize( 11 );
    m_lovedLabel->setFont( f );
    m_lovedLabel->setStyleSheet( "QLabel { color: rgba( 0, 0, 0, 50% ) }" );
    m_lovedIcon->setPixmap( ImageRegistry::instance()->pixmap( RESPATH "images/love.svg", QSize( m_lovedIcon->width(), m_lovedIcon->width() ) ) );
    m_lovedLabel->setText( tr( "Marked as Favorite" ) );

    m_infoBox = new QWidget;
    QHBoxLayout* hboxl = new QHBoxLayout;
    TomahawkUtils::unmarginLayout( hboxl );
    hboxl->setSpacing( 8 );
    hboxl->setContentsMargins( 0, 32, 0, 0 );
    hboxl->addWidget( m_lovedIcon );
    hboxl->addWidget( m_lovedLabel );
    m_infoBox->setLayout( hboxl );
    m_infoBox->hide();

    m_resultsBox = new QWidget;
    QVBoxLayout* resultsLayout = new QVBoxLayout;
    TomahawkUtils::unmarginLayout( resultsLayout );
    resultsLayout->setSpacing( 8 );
    resultsLayout->setContentsMargins( 0, 0, 0, 0 );
    resultsLayout->setSizeConstraint( QLayout::SetMinAndMaxSize );
    m_resultsBox->setLayout( resultsLayout );
    m_resultsBox->hide();

    QScrollArea* resultsScrollArea = new QScrollArea;
    resultsScrollArea->setWidgetResizable( false );
    resultsScrollArea->setWidget( m_resultsBox );
    resultsScrollArea->setFrameShape( QFrame::NoFrame );
    resultsScrollArea->setAttribute( Qt::WA_MacShowFocusRect, 0 );
    resultsScrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    resultsScrollArea->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::MinimumExpanding );

    QVBoxLayout* layout = new QVBoxLayout;
    TomahawkUtils::unmarginLayout( layout );
    layout->addWidget( m_playableCover );
    layout->addSpacerItem( new QSpacerItem( 0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed ) );
    layout->addWidget( m_nameLabel );
    layout->addWidget( m_dateLabel );
    layout->addWidget( m_infoBox );
    layout->addSpacerItem( new QSpacerItem( 0, 32, QSizePolicy::Minimum, QSizePolicy::Fixed ) );
    layout->addWidget( resultsScrollArea );
    layout->addSpacerItem( new QSpacerItem( 0, 32, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding ) );
    layout->setStretchFactor( resultsScrollArea, 1 );

    setLayout( layout );

    m_pixmap = TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultTrackImage, TomahawkUtils::Original, QSize( 48, 48 ) );
    m_playableCover->setPixmap( TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultTrackImage, TomahawkUtils::Grid, m_playableCover->size() ) );
}


TrackDetailView::~TrackDetailView()
{
}


void
TrackDetailView::setQuery( const Tomahawk::query_ptr& query )
{
    if ( query.isNull() )
        return;

    if ( !m_query.isNull() )
    {
        disconnect( m_query->track().data(), SIGNAL( updated() ), this, SLOT( onCoverUpdated() ) );
        disconnect( m_query->track().data(), SIGNAL( socialActionsLoaded() ), this, SLOT( onSocialActionsLoaded() ) );
        disconnect( m_query.data(), SIGNAL( resultsChanged() ), this, SLOT( onResultsChanged() ) );
    }

    m_query = query;
    m_playableCover->setQuery( query );
    onCoverUpdated();
    onResultsChanged();

    m_dateLabel->setText( tr( "Unknown Release-Date" ) );
    if ( m_query->track()->albumPtr() && !m_query->track()->albumPtr()->name().isEmpty() )
    {
        m_nameLabel->setType( QueryLabel::Album );
        m_nameLabel->setAlbum( m_query->track()->albumPtr() );
    }
    else
    {
        m_nameLabel->setType( QueryLabel::Artist );
        m_nameLabel->setArtist( m_query->track()->artistPtr() );
    }

    connect( m_query->track().data(), SIGNAL( updated() ), SLOT( onCoverUpdated() ) );
    connect( m_query->track().data(), SIGNAL( socialActionsLoaded() ), SLOT( onSocialActionsLoaded() ) );
    connect( m_query.data(), SIGNAL( resultsChanged() ), SLOT( onResultsChanged() ) );
}


void
TrackDetailView::onCoverUpdated()
{
    if ( m_query->track()->cover( QSize( 0, 0 ) ).isNull() )
    {
        m_playableCover->setPixmap( TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultTrackImage, TomahawkUtils::Grid, m_playableCover->size() ) );
        return;
    }

    m_pixmap = m_query->track()->cover( m_playableCover->size() );
    m_playableCover->setPixmap( m_pixmap );

    setSocialActions();
}


void
TrackDetailView::onSocialActionsLoaded()
{
    Track* track = qobject_cast< Track* >( sender() );
    if ( !track || !m_query || !track->equals( m_query->track() ) )
        return;

    setSocialActions();
}


void
TrackDetailView::setSocialActions()
{
    if ( m_query->track()->loved() )
    {
        m_lovedIcon->setVisible( true );
        m_lovedLabel->setVisible( true );
        m_infoBox->show();
    }
    else
    {
        m_lovedIcon->setVisible( false );
        m_lovedLabel->setVisible( false );
        m_infoBox->hide();
    }
}


void
TrackDetailView::onResultsChanged()
{
    QLayoutItem* child;
    while ( ( child = m_resultsBox->layout()->takeAt( 0 ) ) != 0 )
    {
        delete child->widget();
        delete child;
    }

    if ( !m_query )
        return;

    foreach ( const Tomahawk::result_ptr& result, m_query->results() )
    {
        if ( !result->isOnline() )
            continue;
        QFont f = font();
        f.setWeight( QFont::DemiBold );
        f.setPointSize( 11 );

        QLabel* resolverIcon = new QLabel( this );
        resolverIcon->setFixedWidth( 12 );
        resolverIcon->setPixmap( result->sourceIcon( TomahawkUtils::RoundedCorners, QSize( 12, 12 ) ) );

        QLabel* resolverLabel = new ClickableLabel( this );
        resolverLabel->setFont( f );
        resolverLabel->setStyleSheet( "QLabel { color: rgba( 0, 0, 0, 50% ) }" );
        resolverLabel->setText( QString( "%1 - %2" ).arg( result->track()->artist() ).arg( result->track()->track() ) );

        NewClosure( resolverLabel, SIGNAL( clicked() ), const_cast< AudioEngine* >( AudioEngine::instance() ),
                                     SLOT( playItem( Tomahawk::playlistinterface_ptr, Tomahawk::result_ptr, Tomahawk::query_ptr ) ),
                                     Tomahawk::playlistinterface_ptr(), result, m_query )->setAutoDelete( false );

        QWidget* hbox = new QWidget;
        QHBoxLayout* hboxl = new QHBoxLayout;
        TomahawkUtils::unmarginLayout( hboxl );
        hboxl->setSpacing( 8 );
        hboxl->addWidget( resolverIcon );
        hboxl->addWidget( resolverLabel );
        hbox->setLayout( hboxl );

        m_resultsBox->layout()->addWidget( hbox );
    }

    if ( m_query->numResults() )
        m_resultsBox->show();
}