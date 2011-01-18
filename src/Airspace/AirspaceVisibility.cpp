/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
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

#include "Airspace/AirspaceVisibility.hpp"
#include "Airspace/AbstractAirspace.hpp"
#include "SettingsComputer.hpp"

bool
AirspaceVisible::type_visible(const AbstractAirspace& airspace) const
{
  return m_settings.DisplayAirspaces[airspace.get_type()];
}

bool
AirspaceVisible::altitude_visible(const AbstractAirspace& airspace) const
{
  /// @todo airspace visibility did use ToMSL(..., map.Calculated().TerrainAlt); 

  switch (m_settings.AltitudeMode) {
  case ALLON:
    return true;
  case CLIP:
    return (airspace.get_base().get_altitude(m_state) <= fixed(m_settings.ClipAltitude));
  case AUTO:
    return (airspace.get_base().is_below(m_state, fixed(m_settings.airspace_warnings.AltWarningMargin))
            && airspace.get_top().is_above(m_state, fixed(m_settings.airspace_warnings.AltWarningMargin)));
  case ALLBELOW:
    return (airspace.get_base().is_below(m_state, fixed(m_settings.airspace_warnings.AltWarningMargin)));
  case INSIDE:
    return (airspace.get_base().is_below(m_state) && airspace.get_top().is_above(m_state));

  case ALLOFF:
    return false;
  }
  return true;
}
