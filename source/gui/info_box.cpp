// This file is part of CaesarIA.
//
// CaesarIA is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// CaesarIA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with CaesarIA.  If not, see <http://www.gnu.org/licenses/>
//
// Copyright 2012-2014 Dalerank, dalerankn8@gmail.com

#include <cstdio>

#include "info_box.hpp"

#include "gfx/tile.hpp"
#include "core/exception.hpp"
#include "core/gettext.hpp"
#include "gfx/decorator.hpp"
#include "objects/metadata.hpp"
#include "objects/house_level.hpp"
#include "game/resourcegroup.hpp"
#include "core/event.hpp"
#include "texturedbutton.hpp"
#include "gui/label.hpp"
#include "city/city.hpp"
#include "objects/market.hpp"
#include "core/stringhelper.hpp"
#include "good/goodhelper.hpp"
#include "objects/farm.hpp"
#include "objects/entertainment.hpp"
#include "objects/house.hpp"
#include "objects/religion.hpp"
#include "religion/romedivinity.hpp"
#include "objects/warehouse.hpp"
#include "gfx/engine.hpp"
#include "gui/special_orders_window.hpp"
#include "good/goodstore.hpp"
#include "environment.hpp"
#include "groupbox.hpp"
#include "walker/walker.hpp"
#include "objects/watersupply.hpp"
#include "objects/senate.hpp"
#include "core/logger.hpp"
#include "objects/constants.hpp"
#include "events/event.hpp"
#include "game/settings.hpp"
#include "objects/well.hpp"
#include "image.hpp"
#include "core/foreach.hpp"
#include "dictionary.hpp"
#include "gameautopause.hpp"
#include "widgetescapecloser.hpp"

using namespace constants;
using namespace gfx;

namespace gui
{

class InfoboxSimple::Impl
{
public:
  Label* lbBackground;
  Label* lbBlackFrame;
  Label* lbTitle;
  Label* lbText;
  PushButton* btnExit;
  PushButton* btnHelp;
  bool isAutoPosition;
  GameAutoPause autopause;

  Impl() : lbBackground(0), lbBlackFrame(0), lbTitle(0),
    lbText(0), btnExit(0), btnHelp(0), isAutoPosition(false)
  {

  }
};

InfoboxSimple::InfoboxSimple( Widget* parent, const Rect& rect, const Rect& blackArea, int id )
: Widget( parent, id, rect ), _d( new Impl )
{
  _d->autopause.activate();
  WidgetEscapeCloser::insertTo( this );

  // create the title
  Widget::setupUI( GameSettings::rcpath( "/gui/infobox.gui" ) );

  _d->lbTitle = findChildA<Label*>( "lbTitle", true, this );
  _d->btnExit = findChildA<TexturedButton*>( "btnExit", true, this );
  _d->btnHelp = findChildA<TexturedButton*>( "btnHelp", true, this );
  _d->lbBackground = findChildA<Label*>( "lbBackground", true, this );
  _d->lbBlackFrame = findChildA<Label*>( "lbBlackFrame", true, this );
  _d->lbText = findChildA<Label*>( "lbText", true, this );

  if( _d->btnExit )
  {
    _d->btnExit->setPosition( Point( width() - 39, height() - 39 ) );
    _d->btnExit->bringToFront();
  }

  if( _d->btnHelp )
  {
    _d->btnHelp->setPosition( Point( 14, height() - 39 ) );
    _d->btnHelp->bringToFront();
  }

  CONNECT( _d->btnExit, onClicked(), this, InfoboxSimple::deleteLater );
  CONNECT( _d->btnHelp, onClicked(), this, InfoboxSimple::showDescription );

  // black box
  Point lastPos( width() - 32, height() - 48 );
  if( _d->lbBlackFrame )
  {
    _d->lbBlackFrame->setVisible( blackArea.size().area() > 0 );
    _d->lbBlackFrame->setGeometry( blackArea );
    lastPos.setY( _d->lbBlackFrame->top() - 10 );
  }

  if( _d->lbText && blackArea.width() == 0 )
  {
    Rect r = _d->lbText->relativeRect();
    r.LowerRightCorner = _d->btnExit->rightupCorner();
    _d->lbText->setGeometry( r );
  }

  _afterCreate();
}

void InfoboxSimple::setText( const std::string& text )
{
  if( _d->lbText ) { _d->lbText->setText( text ); }
}

InfoboxSimple::~InfoboxSimple() {}

void InfoboxSimple::draw(gfx::Engine& engine )
{
  Widget::draw( engine );
}

bool InfoboxSimple::isPointInside( const Point& point ) const
{
  //resolve all screen for self using
  return parent()->absoluteRect().isPointInside( point );
}

bool InfoboxSimple::onEvent( const NEvent& event)
{
  switch( event.EventType )
  {
  case sEventMouse:
    if( event.mouse.type == mouseRbtnRelease )
    {
      deleteLater();
      return true;
    }
    else if( event.mouse.type == mouseLbtnRelease )
    {
      return true;
    }
  break;

  default:
  break;
  }

  return Widget::onEvent( event );
}

void InfoboxSimple::setTitle( const std::string& title )
{
  if( _d->lbTitle ) { _d->lbTitle->setText( title ); }
}

bool InfoboxSimple::isAutoPosition() const{  return _d->isAutoPosition;}
void InfoboxSimple::setAutoPosition( bool value ){  _d->isAutoPosition = value;}

void InfoboxSimple::setupUI(const VariantMap& ui)
{
  Widget::setupUI( ui );

  _d->isAutoPosition = ui.get( "autoPosition", true );
}

Label* InfoboxSimple::_lbTitleRef(){  return _d->lbTitle;}

Label* InfoboxSimple::_lbTextRef(){ return _d->lbText; }
Label* InfoboxSimple::_lbBlackFrameRef(){  return _d->lbBlackFrame; }
PushButton*InfoboxSimple::_btnExitRef() { return _d->btnExit; }

void InfoboxSimple::_updateWorkersLabel(const Point &pos, int picId, int need, int have )
{
  _d->lbBlackFrame->setVisible( need > 0 );
  if( !_d->lbBlackFrame || 0 == need)
    return;

  // number of workers
  std::string text = StringHelper::format( 0xff, "%d %s (%d %s)",
                                           have, _("##employers##"),
                                           need, _("##requierd##") );
  _d->lbBlackFrame->setIcon( Picture::load( ResourceGroup::panelBackground, picId ), Point( 20, 10 ) );
  _d->lbBlackFrame->setText( text );
  _d->lbBlackFrame->setTextOffset( Point( pos.x() + 30, 0 ) );
}


InfoboxBuilding::InfoboxBuilding( Widget* parent, const Tile& tile )
  : InfoboxSimple( parent, Rect( 0, 0, 450, 220 ), Rect( 16, 60, 450 - 16, 60 + 50) )
{
  BuildingPtr building = ptr_cast<Building>( tile.overlay() );
  setTitle( MetaDataHolder::findPrettyName( building->type() ) );
}

}//end namespace gui
