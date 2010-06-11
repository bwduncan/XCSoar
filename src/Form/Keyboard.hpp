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

#ifndef XCSOAR_KEYBOARD_CONTROL_HPP
#define XCSOAR_KEYBOARD_CONTROL_HPP

#include "Screen/ContainerWindow.hpp"

#include <tchar.h>

class WndForm;
class ButtonWindow;

/**
 * The PanelControl class implements the simplest form of a ContainerControl
 */
class KeyboardControl : public ContainerWindow {
public:
  typedef void (*OnCharacterCallback_t)(TCHAR key);

  /**
   * Constructor of the KeyboardControl class
   * @param parent the parent window
   * @param x x-Coordinate of the Control
   * @param y y-Coordinate of the Control
   * @param width Width of the Control
   * @param height Height of the Control
   */
  KeyboardControl(WndForm &form, ContainerWindow &parent, int x, int y,
                  unsigned width, unsigned height,
                  Color background_color, const Font *font,
                  const WindowStyle _style = WindowStyle());

  void SetButtonSize(unsigned width, unsigned height);

  /**
   * Show only the buttons representing the specified character list.
   */
  void SetAllowedCharacters(const TCHAR *allowed);

  void SetOnCharacterCallback(OnCharacterCallback_t Function) {
    mOnCharacter = Function;
  }

protected:
  virtual void on_paint(Canvas &canvas);
  virtual bool on_command(unsigned id, unsigned code);

private:
  Brush background_brush;

  unsigned button_width;
  unsigned button_height;

  WndForm &parent_form;

  ButtonWindow *get_button(const TCHAR* name);
  ButtonWindow *get_button_by_caption(const TCHAR* caption);

  void move_button(const TCHAR* name, int left, int top);
  void resize_button(const TCHAR* name, unsigned int width, unsigned int height);
  void resize_buttons();
  void move_buttons_to_row(const TCHAR* buttons, int row, int offset_left = 0);
  void move_buttons();

  void add_button(WndForm &form, const TCHAR* name,
                  const TCHAR* caption, const Font *font);

  OnCharacterCallback_t mOnCharacter;
};

#endif
