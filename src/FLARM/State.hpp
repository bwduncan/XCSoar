/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

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

#ifndef XCSOAR_FLARM_STATE_HPP
#define XCSOAR_FLARM_STATE_HPP

#include "FLARM/Traffic.hpp"
#include "Sizes.h"

struct FLARM_STATE
{
  /** Number of received FLARM devices */
  unsigned short FLARM_RX;
  /** Transmit status */
  unsigned short FLARM_TX;
  /** GPS status */
  unsigned short FLARM_GPS;
  /** Alarm level of FLARM (0-3) */
  unsigned short FLARM_AlarmLevel;
  /** Is FLARM information available? */
  bool FLARM_Available;
  /** Flarm traffic information */
  FLARM_TRAFFIC FLARM_Traffic[FLARM_MAX_TRAFFIC];
  /**
   * Is there FLARM traffic present?
   * @see FLARM_Traffic
   */
  bool FLARMTraffic;
  /**
   * Is there new FLARM traffic present?
   * @see FLARM_Traffic
   */
  bool NewTraffic;

  /**
   * Looks up an item in the traffic list.
   *
   * @param id FLARM id
   * @return the FLARM_TRAFFIC pointer, NULL if not found
   */
  FLARM_TRAFFIC *FindTraffic(long id) {
    for (unsigned i = 0; i < FLARM_MAX_TRAFFIC; i++)
      if (FLARM_Traffic[i].ID == id)
        return &FLARM_Traffic[i];

    return NULL;
  }

  /**
   * Looks up an item in the traffic list.
   *
   * @param name the name or call sign
   * @return the FLARM_TRAFFIC pointer, NULL if not found
   */
  FLARM_TRAFFIC *FindTraffic(const TCHAR *name) {
    for (unsigned i = 0; i < FLARM_MAX_TRAFFIC; i++)
      if (FLARM_Traffic[i].defined() &&
          _tcscmp(FLARM_Traffic[i].Name, name) == 0)
        return &FLARM_Traffic[i];

    return NULL;
  }

  /**
   * Allocates a new FLARM_TRAFFIC object from the array.
   *
   * @return the FLARM_TRAFFIC pointer, NULL if the array is full
   */
  FLARM_TRAFFIC *AllocateTraffic() {
    for (unsigned i = 0; i < FLARM_MAX_TRAFFIC; i++)
      if (!FLARM_Traffic[i].defined())
        return &FLARM_Traffic[i];

    return NULL;
  }

  void Refresh(fixed Time) {
    bool present = false;

    if (FLARM_Available) {
      for (unsigned i = 0; i < FLARM_MAX_TRAFFIC; i++) {
        if (FLARM_Traffic[i].Refresh(Time))
          present = true;
      }
    }

    FLARMTraffic = present;
    NewTraffic = false;
  }
};

#endif
