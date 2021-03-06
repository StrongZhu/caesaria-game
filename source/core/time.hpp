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

#ifndef __CAESARIA_DATETIME_H_INCLUDE_
#define __CAESARIA_DATETIME_H_INCLUDE_

#include <time.h>

enum class Month {
  january=0, february, march, april,
  may, june, july, august, september,
  october, november, december
};

class DateTime
{
public:
  typedef enum { dateLess=-1, dateEquale=0, dateMore=1 } DATE_EQUALE_FEEL;
  enum { weekInMonth = 4, daysInWeek = 7, monthsInYear = 12, secondsInHour=3600 };

  static const DateTime invalid;

  unsigned char hour() const;
  Month month() const;
  int year() const;
  int get(int prop) const;
  unsigned char minutes() const;
  unsigned char day() const;
  unsigned char dayOfWeek() const;
  unsigned char seconds() const;

  void setHour( unsigned char hour );
  void setMonth( Month month );
  void setYear( unsigned int year );
  void setMinutes( unsigned char minute );
  void setDay( unsigned char day );
  void setSeconds( unsigned char second );
  void setDate(unsigned int year, unsigned int month, unsigned int day);

  explicit DateTime( const char* strValue );
  explicit DateTime( time_t time );

  DateTime( int year, unsigned char month, unsigned char day,
            unsigned char hour=0, unsigned char minute=0, unsigned char sec=0 );

  DateTime( const DateTime& time );

  DateTime();

  DateTime date() const;
  DateTime time() const;

  unsigned int hashdate() const;
  unsigned int hashtime() const;
  static DateTime fromhash( unsigned int hash );

  int daysTo( const DateTime& future ) const;
  int equale( const DateTime& b );
  int monthsTo(const DateTime& end) const;

  DateTime& appendMonth( int month=1 );
  DateTime& appendWeek( int weekNumber=1 );
  DateTime& appendDay( int dayNumber=1 );
  DateTime& operator=( time_t t );
  DateTime& operator=( const DateTime& t );

  static const char* dayName( unsigned char d );
  static const char* monthName(Month d );
  static const char* shortMonthName( Month d );
  static int daysInMonth(int year, int d );
  int daysInMonth() const;
  const char* age() const;

  bool isValid() const;

  bool operator>( const DateTime& other ) const;
  bool operator>=( const DateTime& other ) const;
  bool operator<( const DateTime& other ) const;
  bool operator<=( const DateTime& other ) const;
  bool operator==( const DateTime& other ) const;
  bool operator!=( const DateTime& other ) const;

  static DateTime currenTime();
  static unsigned int elapsedTime();

protected:
  unsigned int _seconds;
  unsigned int _minutes;
  unsigned int _hour;
  unsigned int _day;
  unsigned int _month;
  int _year;

  long _toJd() const;
  void _set( const DateTime& t );

  int _getMonthToDate( const long end ) const;

  int _isEquale( const long b );

  int _getDaysToDate( const long other ) const;
  DateTime _JulDayToDate( const long lJD );
};

class RomanDate : public DateTime
{
public:
  static const int abUrbeCondita = 753;
  const char* age() const;
  static const char* dayName( unsigned char d );
  static const char* monthName( Month d );
  static const char* shortMonthName( unsigned char d );
  explicit RomanDate( const DateTime& date );
};

inline Month operator-(const Month& a, int month )
{
  month %= DateTime::monthsInYear;
  int mIndex = (int)a + ((int)a < month ? DateTime::monthsInYear : 0);
  mIndex -= month;

  return Month( mIndex );
}

#endif //__CAESARIA_DATETIME_H_INCLUDE_
