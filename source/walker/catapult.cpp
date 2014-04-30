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
//
// Copyright 2012-2013 Dalerank, dalerankn8@gmail.com

#include "catapult.hpp"
#include "core/gettext.hpp"
#include "city/city.hpp"
#include "constants.hpp"
#include "game/resourcegroup.hpp"
#include "enemysoldier.hpp"
#include "game/gamedate.hpp"
#include "spear.hpp"

using namespace constants;

namespace {
  const int attackDistance = 16;
}

Catapult::Catapult( PlayerCityPtr city )
  : WallGuard( city, walker::catapult )
{
  _setType( walker::catapult );
  setSpeedMultiplier( 0.f );
  setAttackDistance( 8 );

  _isActive = false;

  setName( _("##catapult##") );
}

CatapultPtr Catapult::create(PlayerCityPtr city)
{
  CatapultPtr ret( new Catapult( city ) );
  ret->drop();

  city->addWalker( ret.object() );

  return ret;
}

Catapult::~Catapult(){}

void Catapult::setActive(bool active)
{
  _isActive = active;
  _setAction( active ? acWork : acNone );
}

bool Catapult::_tryAttack()
{
  EnemySoldierList enemies;
  enemies << _findEnemiesInRange( attackDistance() );

  if( !enemies.empty() )
  {
    //find nearest walkable wall
    EnemySoldierPtr soldierInAttackRange = _findNearbyEnemy( enemies );

    if( soldierInAttackRange.isValid() )
    {
      _setSubAction( fightEnemy );
      fight();
      return true;
    }
  }

  return false;
}

void Catapult::_fire( TilePos target )
{
  SpearPtr spear = Spear::create( _city() );
  spear->setPicInfo( ResourceGroup::sprites, 146 );
  spear->setPicOffset( Point( -15, 15 ));
  spear->toThrow( pos(), target );
  wait( GameDate::days2ticks( 1 ) / 2 );
}

void Catapult::timeStep(const unsigned long time)
{
  WallGuard::timeStep( time );
}

void Catapult::_back2base()
{
  _setSubAction( Soldier::patrol );
  _setAction( acWork );
}
