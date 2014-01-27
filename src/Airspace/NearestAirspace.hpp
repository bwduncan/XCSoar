/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef NEAREST_AIRSPACE_HPP
#define NEAREST_AIRSPACE_HPP

#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "Engine/Airspace/Airspaces.hpp"
#include "Engine/Airspace/AbstractAirspace.hpp"
#include "Geo/GeoPoint.hpp"
#include "NMEA/MoreData.hpp"

class NearestAirspace {

public:
  const AbstractAirspace *airspace;

  /**
   * The horizontal or vertical distance [m], depending on which
   * function filled this object.
   */
  fixed distance;

  NearestAirspace():airspace(NULL) {}
  NearestAirspace(const AbstractAirspace &_airspace, fixed _distance)
    :airspace(&_airspace), distance(_distance) {}

  bool IsDefined() const {
    return airspace != NULL;
  }

  gcc_pure
  static NearestAirspace
  FindHorizontal(const ProtectedAirspaceWarningManager &airspace_warnings,
                 const Airspaces &airspace_database,
                 const MoreData &basic);
};

#endif
