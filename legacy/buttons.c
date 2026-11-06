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

#include "buttons.h"

INCLUDE_EXTERN_RESOURCE(default_button_cross_png);
INCLUDE_EXTERN_RESOURCE(default_button_circle_png);
INCLUDE_EXTERN_RESOURCE(default_button_square_png);
INCLUDE_EXTERN_RESOURCE(default_button_triangle_png);

static vita2d_texture *btn_tex[4] = { NULL, NULL, NULL, NULL };

void initButtons(void) {
  btn_tex[BUTTON_CROSS]   = vita2d_load_PNG_buffer(&_binary_resources_default_button_cross_png_start);
  btn_tex[BUTTON_CIRCLE]  = vita2d_load_PNG_buffer(&_binary_resources_default_button_circle_png_start);
  btn_tex[BUTTON_SQUARE]  = vita2d_load_PNG_buffer(&_binary_resources_default_button_square_png_start);
  btn_tex[BUTTON_TRIANGLE]= vita2d_load_PNG_buffer(&_binary_resources_default_button_triangle_png_start);
}

void finishButtons(void) {
  int i;
  for (i = 0; i < 4; i++) {
    if (btn_tex[i]) {
      vita2d_free_texture(btn_tex[i]);
      btn_tex[i] = NULL;
    }
  }
}

void drawButton(int btn, float x, float y) {
  if (btn >= 0 && btn < 4 && btn_tex[btn]) {
    vita2d_draw_texture(btn_tex[btn], x, y);
  }
}
