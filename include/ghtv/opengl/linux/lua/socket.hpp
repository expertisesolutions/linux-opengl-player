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

#ifndef GHTV_OPENGL_LINUX_LUA_SOCKET_HPP
#define GHTV_OPENGL_LINUX_LUA_SOCKET_HPP

#include <boost/system/error_code.hpp>
#include <string>

namespace ghtv { namespace opengl { namespace linux_ {

struct lua_player;

namespace lua {

class socket
{
public:
  socket(lua_player* player, std::string const& host, int port, int id);
  ~socket();

  void start();
  void stop();
  void join();
  void interrupt();

  bool write(std::string const& value, std::string& error_msg);

private:
  struct socket_impl;
  socket_impl* pimpl;
};

} } } }

#endif
