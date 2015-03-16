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
// Copyright 2012-2014 Dalerank, dalerankn8@gmail.com

#include "city/funds.hpp"
#include "objects/construction.hpp"
#include "city/helper.hpp"
#include "trade_options.hpp"
#include "objects/house.hpp"
#include "objects/constants.hpp"
#include "core/foreach.hpp"
#include "core/logger.hpp"
#include "core/variant_map.hpp"

using namespace constants;

namespace city
{

namespace {
CAESARIA_LITERALCONST(history)
enum { defaultSalary=30 };
}

class Funds::Impl
{
public:
  int taxRate;
  int workerSalary;
  int money;
  int lastYearUpdate;
  int maxDebt;

  IssuesHistory history;

signals public:
  Signal1<int> onChangeSignal;
  Signal1<IssueType> onNewIssueSignal;
};

Funds::Funds() : _d( new Impl )
{
  _d->money = 0;
  _d->workerSalary = defaultSalary;
  _d->lastYearUpdate = 0;
  _d->maxDebt = -5000;
  _d->history.push_back( IssuesValue() );
}

void Funds::resolveIssue( FundIssue issue )
{
  int saveMoney = _d->money;
  bool needUpdateTreasury = true;
  switch( issue.type )
  {
  case unknown:
    Logger::warning( "Funds: wrong issue type %d", issue.type );
    return;
  break;

  case overduePayment:
  case overdueEmpireTax:
    needUpdateTreasury = false;
  break;

  default:
  {
    IssuesValue& step = _d->history.front();
    if( step.find( (IssueType)issue.type ) == step.end() )
    {
      step[ (IssueType)issue.type ] = 0;
    }

    step[ (IssueType)issue.type ] += abs( issue.money );

    _updateCreditDebt(step, issue);

    if( needUpdateTreasury )
    {
      _d->money += issue.money;
    }
  }
  break;
  }

  emit _d->onNewIssueSignal( (IssueType)issue.type );

  if( saveMoney != _d->money )
  {
    emit _d->onChangeSignal( _d->money );
  }
}

void Funds::_updateCreditDebt(IssuesValue& step, FundIssue issue){
    if(issue.type == Funds::taxIncome || issue.type == Funds::exportGoods || issue.type == Funds::donation){
    	 step[ Funds::debet ] += issue.money;
    } else if (issue.type == Funds::importGoods || issue.type == Funds::workersWages || issue.type == Funds::buildConstruction
    		|| issue.type == Funds::creditPercents || issue.type == Funds::playerSalary || issue.type == Funds::sundries
			|| issue.type == Funds::empireTax) {
    	 step[ Funds::credit ] += issue.money;
    }

    step[ Funds::cityProfit ] = step[ Funds::debet ] + step[ Funds::credit ];
}

int Funds::treasury() const { return _d->money; }

int Funds::profit() const
{
  int balanceLastYear = getIssueValue( city::Funds::balance, lastYear );
  return _d->money - balanceLastYear;
}

bool Funds::haveMoneyForAction(unsigned int money)
{
  return (_d->money - (int)money > _d->maxDebt);
}

void Funds::updateHistory( const DateTime& date )
{
  if( _d->lastYearUpdate == date.year() )
  {
    return;
  }

  IssuesValue& step = _d->history.front();
  step[ Funds::balance ] = _d->money;
  step[ Funds::cityProfit ] = profit();

  _d->lastYearUpdate = date.year();
  _d->history.insert( _d->history.begin(), IssuesValue() );

  if( _d->history.size() > 2 )
  {
    _d->history.pop_back();
  }
}

int Funds::getIssueValue( IssueType type, int age ) const
{
  if( (unsigned int)age >= _d->history.size() )
    return 0;

  const IssuesValue& step = _d->history[ age ];
     
  IssuesValue::const_iterator it = step.find( type );
  return ( it == step.end() ) ? 0 : it->second;
}

int Funds::taxRate() const{  return _d->taxRate;}
void Funds::setTaxRate(const unsigned int value) {  _d->taxRate = value;}
int Funds::workerSalary() const{  return _d->workerSalary;}
void Funds::setWorkerSalary(const unsigned int value){  _d->workerSalary = value;}

VariantMap Funds::save() const
{
  VariantMap ret;

  VARIANT_SAVE_ANY_D( ret, _d, money )
  VARIANT_SAVE_ANY_D( ret, _d, taxRate )
  VARIANT_SAVE_ANY_D( ret, _d, workerSalary )
  VARIANT_SAVE_ANY_D( ret, _d, lastYearUpdate )
  
  VariantList history;
  foreach(  stepIt, _d->history )
  {
    VariantList stepHistory;
    foreach( it, *stepIt )
    {
      stepHistory << it->first << it->second;
    }

    history.push_back( stepHistory );
  }

  ret[ lc_history ] = history;

  return ret;
}

void Funds::load( const VariantMap& stream )
{
  VARIANT_LOAD_ANY_D( _d, money, stream )
  VARIANT_LOAD_ANYDEF_D( _d, taxRate, 7, stream )
  VARIANT_LOAD_ANYDEF_D( _d, workerSalary, defaultSalary, stream )
  VARIANT_LOAD_ANY_D( _d, lastYearUpdate, stream )

  VariantList history = stream.get( lc_history ).toList();
  _d->history.clear();
  foreach( it, history )
  {
    _d->history.push_back( IssuesValue() );
    IssuesValue& last = _d->history.back();
    const VariantList& historyStep = (*it).toList();
    VariantList::const_iterator stepIt=historyStep.begin(); 
    while( stepIt != historyStep.end() )
    {
      IssueType type = (IssueType)stepIt->toInt(); ++stepIt;
      int value = stepIt->toInt(); ++stepIt;
      
      last[ type ] = value;
    }
  }
}

Funds::~Funds(){}
Signal1<int>& Funds::onChange(){  return _d->onChangeSignal; }
Signal1<Funds::IssueType>&Funds::onNewIssue(){ return _d->onNewIssueSignal; }

}//end namespace city
