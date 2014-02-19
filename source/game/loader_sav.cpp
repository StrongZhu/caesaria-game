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

#include "loader_sav.hpp"
#include "gfx/tile.hpp"
#include "core/exception.hpp"
#include "core/position.hpp"
#include "objects/objects_factory.hpp"
#include "game.hpp"
#include "pkwareinputstream.hpp"
#include "city/city.hpp"
#include "gfx/tilemap.hpp"
#include "core/logger.hpp"
#include "loaderhelper.hpp"

#include <fstream>
#include <climits>
#include <stdint.h>
#include <map>

class GameLoaderC3Sav::Impl
{
public:
  bool loadCity(std::fstream& f, Game& game);
  void initEntryExit(std::fstream& f, PlayerCityPtr ioCity);
};

GameLoaderC3Sav::GameLoaderC3Sav() : _d( new Impl )
{

}

void SkipCompressed( std::fstream& f )
{
  uint32_t tmp;
  f.read((char*)&tmp, 4);
  f.seekg(tmp, std::ios::cur);
}

void GameLoaderC3Sav::Impl::initEntryExit(std::fstream &f, PlayerCityPtr ioCity)
{
  unsigned int size = ioCity->getTilemap().getSize();

  const std::fstream::pos_type savePos = f.tellg();

  // init road entry/exit point
  unsigned short int i = 0;
  unsigned short int j = 0;
  f.seekg(1236, std::ios::cur);
  f.read((char*)&i, 2);
  f.read((char*)&j, 2);

  BorderInfo borderInfo;

  borderInfo.roadEntry = TilePos( i, size - j - 1 );

  f.read((char*)&i, 2);
  f.read((char*)&j, 2);
  borderInfo.roadExit = TilePos( i, size - j - 1 );

  // init boat entry/exit point
  f.seekg(savePos, std::ios::beg);
  f.seekg(1276, std::ios::cur);
  f.read((char*)&i, 2);
  f.read((char*)&j, 2);
  borderInfo.boatEntry = TilePos( i, size - j - 1 );

  f.read((char*)&i, 2);
  f.read((char*)&j, 2);
  borderInfo.boatExit = TilePos( i, size - j - 1);

  ioCity->setBorderInfo( borderInfo );

  f.seekg(savePos, std::ios::beg);
}


bool GameLoaderC3Sav::load(const std::string& filename, Game& game)
{
  std::fstream f(filename.c_str(), std::ios::in | std::ios::binary);

  //_d->initClimate(f, game.getCity() );

  _d->loadCity(f, game );

  f.close();

  return true;
}

bool GameLoaderC3Sav::Impl::loadCity( std::fstream& f, Game& game )
{ 
  uint32_t tmp;

  // need to rewrite better
  short int     *pGraphicGrid = (short int     *)malloc(52488);
  unsigned char *pEdgeGrid    = (unsigned char *)malloc(26244);
  short int     *pTerrainGrid = (short int     *)malloc(52488);
  unsigned char *pRndmTerGrid = (unsigned char *)malloc(26244);
  unsigned char *pRandomGrid  = (unsigned char *)malloc(26244);
  unsigned char *pZeroGrid    = (unsigned char *)malloc(26244);
  
  if( pGraphicGrid == NULL || pEdgeGrid == NULL || pTerrainGrid == NULL ||
      pRndmTerGrid == NULL || pRandomGrid == NULL || pZeroGrid == NULL )
  {
    THROW("NOT ENOUGH MEMORY!!!! FATAL");
  }    
  
  if (!f.is_open())
    THROW("can't open file");
  
  f.read((char*)&tmp, 4); // read dummy

  std::string cityName = LoaderHelper::getDefaultCityName( tmp );
  game.getCity()->setName( cityName );
  
  f.read((char*)&tmp, 4); // read scenario flag
  
  try
  {
    f.read((char*)&tmp, 4); // read length of compressed chunk
    std::cout << "length of compressed ids is " << tmp << std::endl;
    PKWareInputStream *pk = new PKWareInputStream(&f, false, tmp);
    for (int i = 0; i < 162 * 162; i++)
    {
      pGraphicGrid[i] = pk->readShort();
    }
    pk->empty();
    delete pk;
    
    f.read((char*)&tmp, 4); // read length of compressed chunk
    std::cout << "length of compressed egdes is " << tmp << std::endl;
    pk = new PKWareInputStream(&f, false, tmp);
    for (int i = 0; i < 162 * 162; i++)
    {
      pEdgeGrid[i] = pk->readByte();
    }
    pk->empty();
    delete pk;
    
    SkipCompressed(f); // skip building ids
    
    f.read((char*)&tmp, 4); // read length of compressed chunk
    std::cout << "length of compressed terraindata is " << tmp << std::endl;
    pk = new PKWareInputStream(&f, false, tmp);
    for (int i = 0; i < 162 * 162; i++)
    {
      pTerrainGrid[i] = pk->readShort();
    }
    pk->empty();
    delete pk;
    
    SkipCompressed(f);
    SkipCompressed(f);
    SkipCompressed(f);
    SkipCompressed(f);
    
    f.read((char*)pRandomGrid, 26244);
    
    SkipCompressed(f);
    SkipCompressed(f);
    SkipCompressed(f);
    SkipCompressed(f);
    SkipCompressed(f);
    
    // here goes walkers array
    f.read((char*)&tmp, 4); // read length of compressed chunk
    std::cout << "length of compressed walkers data is " << tmp << std::endl;
    pk = new PKWareInputStream(&f, false, tmp);    
    for (int j = 0; j < 1000; j++)
    {
      pk->skip(10);
      pk->readShort();
      pk->skip(8);
      pk->readByte();
      pk->readByte();
      pk->skip(106);
    }
    pk->empty();
    delete pk;
    int length;
    f.read((char*)&length, 4); // read next length :-)
    if (length <= 0)
      f.seekg(1200, std::ios::cur);
    else
      f.seekg(length, std::ios::cur);
    
    SkipCompressed(f);
    SkipCompressed(f);

    // 3x int
    f.read((char*)&tmp, 4);
    f.read((char*)&tmp, 4);
    f.read((char*)&tmp, 4);
    SkipCompressed(f);
    f.seekg(70, std::ios::cur);
    SkipCompressed(f); // skip building list
    f.seekg(208, std::ios::cur);
    SkipCompressed(f); // skip unknown
    f.seekg(788, std::ios::cur); // skip unused data
    f.read((char*)&tmp, 4); //mapsize

    int size = tmp;
    PlayerCityPtr oCity = game.getCity();
    Tilemap& oTilemap = oCity->getTilemap();

    oTilemap.resize(size);
    oCity->setCameraPos( TilePos( 0, 0 ) );

    initEntryExit( f, game.getCity() );

    f.seekg(1312, std::ios::cur);
    char climate;
    f.read(&climate, 1);
    oCity->setClimate((ClimateType)climate);
    
    // here goes the WORK!
    
    
  // loads the graphics map
  int border_size = (162 - size) / 2;

  std::map< int, std::map< int, unsigned char > > edgeData;

  game.getCity()->setCameraPos( TilePos( size/2, size/2 ) );

  for (int itA = 0; itA < size; ++itA)
  {
    for (int itB = 0; itB < size; ++itB)
    {
      int i = itB;
      int j = size - itA - 1;

      int index = 162 * (border_size + itA) + border_size + itB;

      Tile& tile = oTilemap.at(i, j);
      tile.setPicture( TileHelper::convId2PicName( pGraphicGrid[index] ) );
      tile.setOriginalImgId( pGraphicGrid[index] );

      edgeData[ i ][ j ] = pEdgeGrid[index];
      TileHelper::decode( tile, pTerrainGrid[index] );
    }
  }    

  for (int i = 0; i < size; ++i)
  {
    for (int j = 0; j < size; ++j)
    {
      unsigned char ed = edgeData[ i][ j ];
      if( ed == 0x00)
      {
        int size = 1;

				{
					int dj;
					try
					{
						// find size, 5 is maximal size for building
						for (dj = 0; dj < 5; ++dj)
						{
							int edd = edgeData[ i ][ j - dj ];
							// find bottom left corner
							if (edd == 8 * dj + 0x40)
							{
								size = dj + 1;
								break;
							}
						}
					}
					catch(...)
					{
						size = dj + 1;
					}
				}

				Logger::warning( "Multi-tile x %d at (%d,%d)", size, i, j );

				Tile& master = oTilemap.at(i, j - size + 1);

				Logger::warning( "Master will be at (%d,%d)", master.i(), master.j() );

        for (int di = 0; di < size; ++di)
        {
          for (int dj = 0; dj < size; ++dj)
          {
            oTilemap.at(master.i() + di, master.j() + dj).setMasterTile(&master);
          }
        }

        Logger::warning( " decoding " );
      }

      // Check if it is building and type of building
      //if (ttile.getMasterTile() == NULL)
      LoaderHelper::decodeTerrain( oTilemap.at( i, j ), oCity );
    }
  }
    
  }
  catch(PKException)
  {
    THROW("fatal error when unpacking");
  }

  return true;
}

bool GameLoaderC3Sav::isLoadableFileExtension( const std::string& filename )
{
  return filename.substr( filename.size() - 4, -1 ) == ".sav";
}
