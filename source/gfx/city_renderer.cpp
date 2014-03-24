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
// along with CaesarIA.  If not, see <http://www.gnu.org/licenses/>.

#include "city_renderer.hpp"

#include <list>
#include <vector>

#include "tile.hpp"
#include "engine.hpp"
#include "game/resourcegroup.hpp"
#include "core/position.hpp"
#include "pictureconverter.hpp"
#include "core/event.hpp"
#include "gfx/renderermode.hpp"
#include "gfx/tilemap.hpp"
#include "core/stringhelper.hpp"
#include "objects/house_level.hpp"
#include "core/foreach.hpp"
#include "events/event.hpp"
#include "core/font.hpp"
#include "gfx/sdl_engine.hpp"
#include "core/gettext.hpp"
#include "core/logger.hpp"
#include "layersimple.hpp"
#include "layerwater.hpp"
#include "layerfire.hpp"
#include "layerfood.hpp"
#include "layerhealth.hpp"
#include "layerconstants.hpp"
#include "layerreligion.hpp"
#include "layerbuild.hpp"
#include "layerdamage.hpp"
#include "layerdesirability.hpp"
#include "layerentertainment.hpp"
#include "layertax.hpp"
#include "layercrime.hpp"
#include "walker/walker.hpp"
#include "objects/aqueduct.hpp"
#include "layerdestroy.hpp"
#include "tilemap_camera.hpp"
#include "layereducation.hpp"
#include "city/city.hpp"
#include "layertroubles.hpp"

using namespace constants;

class CityRenderer::Impl
{
public: 
  typedef std::vector<LayerPtr> Layers;

  PlayerCityPtr city;     // city to display
  Tilemap* tilemap;
  gui::GuiEnv* guienv;
  GfxEngine* engine;
  TilemapCamera camera;  // visible map area
  Layers layers;
  Point currentCursorPos;

  Renderer::ModePtr changeCommand;

  LayerPtr currentLayer;
  void setLayer( int type );
};

CityRenderer::CityRenderer() : _d( new Impl )
{
}

CityRenderer::~CityRenderer() {}

void CityRenderer::initialize(PlayerCityPtr city, GfxEngine* engine)
{
  _d->city = city;
  _d->tilemap = &city->getTilemap();
  _d->camera.init( *_d->tilemap );
  _d->engine = engine;

  addLayer( LayerSimple::create( _d->camera, city ) );
  addLayer( LayerWater::create( _d->camera, city ) );
  addLayer( LayerFire::create( _d->camera, city ) );
  addLayer( LayerFood::create( _d->camera, city ) );
  addLayer( LayerHealth::create( _d->camera, city, citylayer::health ));
  addLayer( LayerHealth::create( _d->camera, city, citylayer::doctor ));
  addLayer( LayerHealth::create( _d->camera, city, citylayer::hospital ));
  addLayer( LayerHealth::create( _d->camera, city, citylayer::barber ));
  addLayer( LayerHealth::create( _d->camera, city, citylayer::baths ));
  addLayer( LayerReligion::create( _d->camera, city ) );
  addLayer( LayerDamage::create( _d->camera, city ) );
  addLayer( LayerDesirability::create( _d->camera, city ) );
  addLayer( LayerEntertainment::create( _d->camera, city, citylayer::entertainment ) );
  addLayer( LayerEntertainment::create( _d->camera, city, citylayer::theater ) );
  addLayer( LayerEntertainment::create( _d->camera, city, citylayer::amphitheater ) );
  addLayer( LayerEntertainment::create( _d->camera, city, citylayer::colloseum ) );
  addLayer( LayerEntertainment::create( _d->camera, city, citylayer::hippodrome ) );
  addLayer( LayerCrime::create( _d->camera, city ) ) ;
  addLayer( LayerBuild::create( this, city ) );
  addLayer( LayerDestroy::create( _d->camera, city ) );
  addLayer( LayerTax::create( _d->camera, city ) );
  addLayer( LayerEducation::create( _d->camera, city, citylayer::education ) );
  addLayer( LayerEducation::create( _d->camera, city, citylayer::school ) );
  addLayer( LayerEducation::create( _d->camera, city, citylayer::library ) );
  addLayer( LayerEducation::create( _d->camera, city, citylayer::academy ) );
  addLayer( LayerTroubles::create( _d->camera, city ) );

  _d->setLayer( citylayer::simple );
}

void CityRenderer::Impl::setLayer(int type)
{
  currentLayer = 0;
  foreach( layer, layers )
  {
    if( (*layer)->getType() == type )
    {
      currentLayer = *layer;
      break;
    }
  }

  if( currentLayer.isNull() )
  {
    currentLayer = layers.front();
  }

  currentLayer->init( currentCursorPos );
}

void CityRenderer::render()
{
  if( _d->currentLayer.isNull() )
  {
    return;
  }

  _d->currentLayer->beforeRender( *_d->engine );

  _d->currentLayer->render( *_d->engine );

  _d->currentLayer->renderPass( *_d->engine, Renderer::animations );

  _d->currentLayer->afterRender( *_d->engine );

  if( _d->currentLayer->getType() != _d->currentLayer->getNextLayer() )
  {
    _d->setLayer( _d->currentLayer->getNextLayer() );
  }
}

void CityRenderer::handleEvent( NEvent& event )
{
  if( event.EventType == sEventMouse )
  {
    _d->currentCursorPos = event.mouse.pos();
  }

  _d->currentLayer->handleEvent( event );
}

void CityRenderer::setMode( Renderer::ModePtr command )
{
  _d->changeCommand = command;

  LayerModePtr ovCmd = ptr_cast<LayerMode>( _d->changeCommand );
  if( ovCmd.isValid() )
  {
    _d->setLayer( ovCmd->getType() );
  }
}

void CityRenderer::animate(unsigned int time)
{
  const TilesArray& visibleTiles = _d->camera.getTiles();

  for( TilesArray::const_iterator i=visibleTiles.begin(); i != visibleTiles.end(); i++ )
  {
    (*i)->animate( time );
  }
}

TilemapCamera& CityRenderer::camera() {  return _d->camera; }
Renderer::ModePtr CityRenderer::getMode() const {  return _d->changeCommand;}
void CityRenderer::addLayer(LayerPtr layer){  _d->layers.push_back( layer ); }
TilePos CityRenderer::getTilePos( Point point ) const
{
  return _d->camera.at( point, true )->pos();
}
Tilemap& CityRenderer::getTilemap(){   return *_d->tilemap; }
