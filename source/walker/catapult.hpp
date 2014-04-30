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

#ifndef _CAESARIA_CATAPULT_INCLUDE_H_
#define _CAESARIA_CATAPULT_INCLUDE_H_

#include "wallguard.hpp"
#include "core/predefinitions.hpp"

class Catapult : public WallGuard
{
public:
  static CatapultPtr create( PlayerCityPtr city );

  virtual ~Catapult();

  void setActive( bool active );
  virtual void timeStep(const unsigned long time);

protected:
  Catapult( PlayerCityPtr city );
  virtual void _back2base();
  virtual bool _tryAttack();
  virtual void _fire(TilePos target);

private:
  bool _isActive;
};

#endif //_CAESARIA_CATAPULT_INCLUDE_H_
