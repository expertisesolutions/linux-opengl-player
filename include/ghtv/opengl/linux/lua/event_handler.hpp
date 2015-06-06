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

#ifndef EVENT_HANDLER_HPP
#define EVENT_HANDLER_HPP

#include <luabind/object.hpp>
#include <string>

namespace ghtv { namespace opengl { namespace linux_ { namespace lua {

struct event_handler
{
  // luabind::type(handler) == LUA_TFUNCTION
  luabind::object handler;
  // if class_.empty() then filters.empty()
  std::string class_;
  std::vector<luabind::object> filters;
};

} } } }

#endif // EVENT_HANDLER_HPP
