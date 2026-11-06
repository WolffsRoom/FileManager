/*
  VitaShell
  Copyright (C) 2015-2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include "main.h"

#define BUTTON_CROSS 0
#define BUTTON_CIRCLE 1
#define BUTTON_SQUARE 2
#define BUTTON_TRIANGLE 3

#define BUTTON_SIZE 20

void initButtons(void);
void finishButtons(void);
void drawButton(int btn, float x, float y);

#endif
