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

#ifndef EVENTS_HPP
#define EVENTS_HPP

#include <luabind/object.hpp>
#include <string>

namespace ghtv { namespace opengl { namespace linux_ {

struct lua_player;

namespace lua {
namespace event {

luabind::object timer(int time, luabind::object f, lua_player* player);

int uptime(lua_player* player);

int post(lua_State* L);

int register_(lua_State* L);

} } } } }



#endif // EVENTS_HPP
