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

#include <ghtv/opengl/linux/lua/timer.hpp>

#include <ghtv/opengl/linux/lua_player.hpp>
#include <boost/thread/locks.hpp>

namespace ghtv { namespace opengl { namespace linux_ { namespace lua {

// TODO Move arguments from constructor to here?
void timer::start(luabind::object& f)
{
  thread = player->create_lua_thread();
#ifndef NDEBUG
  int old_top = lua_gettop(thread);
#endif
  //function.push(thread);
  f.push(f.interpreter());
  lua_xmove(f.interpreter(), thread, 1);
  assert(old_top + 1 == lua_gettop(thread));

  timer_thread = boost::thread(&timer::count_down, this);
}

void timer::cancel()
{
  cancel_condition.notify_all();
}

void timer::count_down()
{
  try
  {
    {
      // TODO Precise time?
      boost::unique_lock<boost::mutex> lock(cancel_mutex);
      if(cancel_condition.timed_wait(lock, boost::posix_time::millisec(time)))
      {
        std::cout << "Lua timer event cancelled..." << std::endl;
        destroy();
        return;
      }
    }

    player->add_lua_thread_to_execution_queue(thread);

    boost::this_thread::interruption_point();

    destroy();
  }
  catch (boost::thread_interrupted& interruption)
  {
    //std::cerr << "Unexpected interruption of lua timer event..." << std::endl;
    std::cout << "Lua timer interrupted..." << std::endl;
  }
  catch(...)
  {
    std::cerr << "Error: Unexpected exception on lua timer..." << std::endl;
  }
}

void timer::destroy()
{
  boost::lock_guard<boost::mutex> lock(player->timers_mutex);

  boost::this_thread::interruption_point();

  // Remove itself from the player timers list
  std::vector<timer*>::iterator
    iterator = std::find(player->timers.begin(), player->timers.end(), this);
  assert(iterator != player->timers.end());
  assert(*iterator == this);
  player->timers.erase(iterator);

  // Commit suicide
  timer_thread.detach();
  delete this;
}

} } } }
