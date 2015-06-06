/* (c) Copyright 2011-2014 Felipe Magno de Almeida
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ghtv/opengl/linux/lua_player.hpp>

#include <ghtv/opengl/linux/lua/canvas.hpp>

#include <luabind/luabind.hpp>
#include <luabind/object.hpp>
#include <luabind/out_value_policy.hpp>

#include <iostream>

namespace ghtv { namespace opengl { namespace linux_ {

void lua_player::init_canvas()
{
  using lua::canvas;

  luabind::module(L, "ghtv")
  [
   luabind::class_<canvas>("canvas")
     .def("new", &canvas::canvas_image_new)
     .def("new", &canvas::canvas_buffer_new)
     .def("attrSize", &canvas::attrSize, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3))
     .def("attrColor", &canvas::attrColor)
     .def("attrColor", &canvas::attrColor_string)
     .def("attrColor", &canvas::get_attrColor, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3) | luabind::pure_out_value(_4)
          | luabind::pure_out_value(_5))
     .def("attrFont", &canvas::attrFont)
     .def("attrFont", &canvas::attrFont_non_normative)
     .def("attrFont", &canvas::get_attrFont)
     .def("attrClip", &canvas::attrClip)
     .def("attrClip", &canvas::get_attrClip, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3) | luabind::pure_out_value(_4)
          | luabind::pure_out_value(_5))
     .def("attrCrop", &canvas::attrCrop)
     .def("attrCrop", &canvas::get_attrCrop, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3) | luabind::pure_out_value(_4)
          | luabind::pure_out_value(_5))
     .def("attrFlip", &canvas::attrFlip)
     .def("attrFlip", &canvas::get_attrFlip, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3))
     .def("attrOpacity", &canvas::attrOpacity)
     .def("attrOpacity", &canvas::get_attrOpacity)
     .def("attrRotation", &canvas::attrRotation)
     .def("attrRotation", &canvas::get_attrRotation)
     .def("attrScale", (void(canvas::*)(int, int)) &canvas::attrScale)
     .def("attrScale", (void(canvas::*)(bool, int)) &canvas::attrScale)
     .def("attrScale", (void(canvas::*)(int, bool)) &canvas::attrScale)
     .def("attrScale", (void(canvas::*)(bool, bool)) &canvas::attrScale)
     .def("attrScale", &canvas::get_attrScale, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3))
     .def("drawLine", &canvas::drawLine)
     .def("drawRect", &canvas::drawRect)
     .def("drawRoundRect", &canvas::drawRoundRect)
     .def("drawPolygon", &canvas::drawPolygon)
     .def("drawEllipse", &canvas::drawEllipse)
     .def("drawText", &canvas::drawText)
     .def("clear", &canvas::clear)
     .def("clear", &canvas::clear_default)
     .def("flush", &canvas::flush)
     .def("compose", (void(canvas::*)(int, int, canvas)) &canvas::compose)
     .def("compose", (void(canvas::*)(int, int, canvas, int, int, int, int)) &canvas::compose)
     .def("pixel", &canvas::pixel)
     .def("pixel", &canvas::get_pixel, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3) | luabind::pure_out_value(_4)
          | luabind::pure_out_value(_5))
     .def("measureText", &canvas::measureText, luabind::pure_out_value(_2)
          | luabind::pure_out_value(_3))
  ];

  // Registering root canvas.
  lua::canvas tmp_canvas(width, height, this);
  luabind::globals(L)["canvas"] = tmp_canvas;
  root_canvas = luabind::object_cast<lua::canvas*>(luabind::globals(L)["canvas"]);
}

} } }
