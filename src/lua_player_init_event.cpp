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

#include <ghtv/opengl/linux/lua/event.hpp>

#include <lua.hpp>
#include <luabind/luabind.hpp>
#include <luabind/object.hpp>
#include <luabind/tag_function.hpp>
#include <luabind/out_value_policy.hpp>
#include <luabind/scope.hpp>

#include <iostream>

namespace ghtv { namespace opengl { namespace linux_ {

void lua_player::init_event()
{
  using luabind::tag_function;
  luabind::module(L, "event")
  [
   /*luabind::def("post", tag_function<bool(luabind::object, std::string&)>
                (boost::bind(&lua::event::post_out, _1, _2, this))
                , luabind::pure_out_value(_2))
   , luabind::def("post", tag_function<bool(std::string, luabind::object, std::string&)>
                (boost::bind(&lua::event::post, _1, _2, _3, this))
                , luabind::pure_out_value(_3))
   ,*/ luabind::def("timer", tag_function<luabind::object(int, luabind::object)>
                  (boost::bind(&lua::event::timer, _1, _2, this)))
   , luabind::def("uptime", tag_function<int()>(boost::bind(&lua::event::uptime, this)))
  ];

  lua_pushcfunction(L, &lua::event::post);
  luabind::object post(luabind::from_stack(L, -1));
  luabind::globals(L)["event"]["post"] = post;

  lua_pushcfunction(L, &lua::event::register_);
  luabind::object register_(luabind::from_stack(L, -1));
  luabind::globals(L)["event"]["register"] = register_;
}

} } }
