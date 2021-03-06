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
// Copyright 2012-2015 Dalerank, dalerankn8@gmail.com

#include "tilepos_array.hpp"
#include "gfx/tilepos.hpp"
#include "variant_list.hpp"

TilePosArray& TilePosArray::append(int i, int j)
{
  push_back( TilePos( i, j ) );
  return *this;
}

VariantList TilePosArray::save() const
{
  VariantList ret;
  for( const auto& pos : *this )
    { ret << pos; }
  return ret;
}

void TilePosArray::load(const VariantList &vlist)
{
  clear();
  for( const auto& it : vlist )
    push_back( it.toTilePos() );
}
