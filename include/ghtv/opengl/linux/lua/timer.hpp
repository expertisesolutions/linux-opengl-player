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

#ifndef TIMER_HPP
#define TIMER_HPP

#include <luabind/object.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace ghtv { namespace opengl { namespace linux_ {

struct lua_player;

namespace lua {

struct timer
{
  timer(int time, lua_player* player)
    : time(time)
    , player(player)
    , thread(0)
  {}
  ~timer(){}

  void start(luabind::object& f);
  void cancel();

  boost::thread timer_thread;

private:
  void count_down();
  void destroy();

  int time;
  lua_player* player;
  lua_State* thread;

  boost::mutex cancel_mutex;
  boost::condition_variable cancel_condition;
};

} } } }

#endif // TIMER_HPP
