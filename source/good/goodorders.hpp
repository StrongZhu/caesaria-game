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


#ifndef __CAESARIA_GOODORDERS_H_INCLUDED__
#define __CAESARIA_GOODORDERS_H_INCLUDED__

#include "good.hpp"
#include "core/scopedptr.hpp"

class GoodOrders 
{
public:
  typedef enum { accept=0, reject, deliver, none } Order;
  
  GoodOrders();
  ~GoodOrders();

  void set( const Good::Type type, Order rule );
  void set( Order rule );
  Order get( const Good::Type type );

private:
  class Impl;
  ScopedPtr< Impl > _d;
};

#endif //__CAESARIA_GOODORDERS_H_INCLUDED__
