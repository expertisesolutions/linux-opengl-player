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

#include <ghtv/opengl/linux/lua/event.hpp>

#include <ghtv/opengl/linux/lua_player.hpp>
#include <ghtv/opengl/linux/lua/timer.hpp>
#include <ghtv/opengl/linux/lua/socket.hpp>
#include <ghtv/opengl/linux/global_state.hpp>
#include <ghtv/opengl/linux/idle_update.hpp>
#include <gntl/transition_enum.hpp>
#include <luabind/luabind.hpp>
#include <luabind/tag_function.hpp>
#include <luabind/make_function.hpp>

#include <iostream>

namespace ghtv { namespace opengl { namespace linux_ { namespace lua { namespace event {

namespace {

int cancel_timer_closure(lua_State *context)
{
  // TODO check if timer is still alive
  lua::timer* ptimer = (lua::timer*) lua_topointer(context, lua_upvalueindex(1));
  ptimer->cancel();
  return 0;
}

void cxx_register(lua_State* L, int pos, luabind::object handler
                    , std::string class_
                    , std::vector<luabind::object> filters, lua_player* player)
{
  std::vector<lua::event_handler>::iterator iterator = player->event_handlers.end();
  std::size_t unsigned_pos = pos;
  if(pos >= 0 && unsigned_pos <= player->event_handlers.size())
  {
    iterator = boost::next(player->event_handlers.begin(), pos);
  }

  handler.push(handler.interpreter());
  lua_xmove(handler.interpreter(), L, 1);
  luabind::object handler_(luabind::from_stack(L, -1));

  filters.clear();

  lua::event_handler h = {handler_, class_, filters};
  player->event_handlers.insert(iterator, h);
}

bool post_out_ncl(luabind::object event, std::string& error_msg, lua_player* player)
{
  std::string type_ = luabind::object_cast<std::string>(event["type"]);

  gntl::event_type ev_t = type_ == "presentation" ? gntl::event_enum::presentation :
                          type_ == "selection" ? gntl::event_enum::selection :
                          type_ == "attribution" ? gntl::event_enum::attribution :
                          throw std::runtime_error("\"" + type_ + "\" is not a valid type for ncl class events");

  boost::optional<std::string> area;
  if(ev_t == gntl::event_enum::attribution)
  {
    // TODO
    error_msg = "Attribution type events not implemented yet!";
    return false;
  }
  else
  {
    if(event["label"]) {
      area = luabind::object_cast<std::string>(event["label"]);
    }
  }

  gntl::transition_type trans = gntl::transition_enum::starts;
  if(event["action"])
  {
    luabind::object action = event["action"];
    if(luabind::type(action) == LUA_TSTRING)
    {
      std::string a = luabind::object_cast<std::string>(action);
      if(a == "start")
        ;
      else if(a == "stop")
        trans = gntl::transition_enum::stops;
      else if(a == "abort")
        trans = gntl::transition_enum::aborts;
      else if(a == "pause")
        trans = gntl::transition_enum::pauses;
      else if(a == "resume")
        trans = gntl::transition_enum::resumes;
    }
  }

  typedef gntl::concept::structure::document_traits
    <state::main_state_type::document_type> document_traits;

  // TODO Always using the top level document, get the correct one.
  document_traits::register_event(*global_state.main_state.document, player->identifier, area, trans, ev_t);

  ghtv::opengl::linux_::async_signal_new_event();

  return true;
}

bool post_out_tcp(luabind::object event, std::string& error_msg, lua_player* player)
{
  if(!event["type"])
  {
    error_msg = "Event field \"type\" not found";
    return false;
  }

  std::string type = luabind::object_cast<std::string>(event["type"]);

  if(type == "connect")
  {
    std::string host = luabind::object_cast<std::string>(event["host"]);
    int port = luabind::object_cast<int>(event["port"]);

    // TODO timeout

    {
      boost::lock_guard<boost::mutex> lock(player->sockets_mutex);
      player->create_socket(host, port);
    }
    return true;
  }
  else if(type == "data")
  {
    int connection_id = luabind::object_cast<int>(event["connection"]);
    std::string value = luabind::object_cast<std::string>(event["value"]);

    lua::socket* socket = 0;
    {
      boost::lock_guard<boost::mutex> lock(player->sockets_mutex);
      lua_player::socket_cont::iterator it = player->sockets.find(connection_id);
      if(it != player->sockets.end()) {
        socket = it->second;
      }
    }
    if(!socket) {
      error_msg = "Invalid connection identifier";
      return false;
    }
    return socket->write(value, error_msg);
  }
  else if(type == "disconnect")
  {
    int connection_id = luabind::object_cast<int>(event["connection"]);
    lua_player::socket_cont::iterator it = player->sockets.find(connection_id);
    if(it == player->sockets.end()) {
      error_msg = "Invalid connection identifier";
      return false;
    }

    lua::socket* socket = it->second;
    socket->stop();
    return true;
  }
  else
  {
    error_msg = "Unknown type \"" + type + "\" for tcp class";
    return false;
  }
}

bool post_out(luabind::object event, std::string& error_msg, lua_player* player)
{
  try
  {
    if(!event["class"]) {
      error_msg = "Event field \"class\" not found.";
      return false;
    }

    std::string class_ = luabind::object_cast<std::string>(event["class"]);

    if(class_ == "ncl")
    {
      return post_out_ncl(event, error_msg, player);
    }
    else if(class_ == "tcp")
    {
      return post_out_tcp(event, error_msg, player);
    }
    else
    {
      error_msg = "Unknown event class \"" + class_ + "\"";
      std::cerr << error_msg << std::endl;
      return false;
    }
  }
  catch(luabind::error const& e)
  {
    error_msg = "Error while running event.post: ";
    if(lua_gettop(e.state()) != 0)
      error_msg += lua_tostring(e.state(), -1);
    std::cerr << error_msg << std::endl;
  }
  catch(std::exception& e)
  {
    error_msg = "Exception thrown while running event.post: ";
    error_msg += e.what();
    std::cerr << error_msg << std::endl;
  }
  return false;
}

#if 0
bool post(std::string dst, luabind::object event, std::string& error_msg, lua_player* player)
{
  if(dst == "out") {
    return post_out(event, error_msg, player);
  } else if(dst == "in") {
    // TODO
    error_msg = "post \"in\" is not implemented yet";
  } else {
    error_msg = "\"" + dst + "\" is not a valid post dst parameter";
  }
  return false;
}
#endif

} // end of anonymous namespace

int post(lua_State* L)
{
  if(lua_gettop(L) < 1)
  {
    lua_pushstring(L, "Missing \"event\" argument");
    lua_error(L); // Long jump. Never returns!
  }

  int ret = 0;
  std::string error_msg;
  try
  {
    lua_player* player = luabind::object_cast<lua_player*>(luabind::registry(L)["lua_player"]);
    luabind::object first(luabind::from_stack(L, -1));
    if(luabind::type(first) != LUA_TSTRING)
    {
      ret = post_out(first, error_msg, player);
    }
    else
    {
      if(lua_gettop(L) < 2)
      {
        lua_pushstring(L, "Missing \"event\" argument");
        lua_error(L); // Long jump. Never returns!
      }
      std::string dst = luabind::object_cast<std::string>(first);
      luabind::object event(luabind::from_stack(L, -2));
      if(dst == "out")
      {
        ret = post_out(event, error_msg, player);
      }
      else if(dst == "in")
      {
        // TODO
        error_msg = "post \"in\" is not implemented yet";
      }
      else
      {
        error_msg = "\"" + dst + "\" is not a valid post dst parameter";
      }
    }
  }
  catch(luabind::error const& e)
  {
    std::string error_msg = "Error running function event.post: ";
    if(lua_gettop(e.state()) != 0)
      error_msg += lua_tostring(e.state(), -1);
    std::cerr << error_msg << std::endl;
  }

  lua_pushboolean(L, ret);
  lua_pushstring(L, error_msg.c_str());
  return 2;
}

luabind::object timer(int time, luabind::object f, lua_player* player)
{
  if(luabind::type(f) != LUA_TFUNCTION)
  {
    lua_pushnil(f.interpreter());
    luabind::object nil(luabind::from_stack(f.interpreter (), -1));
    return nil;
  }
  else
  {
    boost::lock_guard<boost::mutex> lock(player->timers_mutex);

    lua::timer* timer = new lua::timer(time, player);
    player->timers.push_back(timer);

    lua_pushlightuserdata(f.interpreter(), timer);              // ... [timer]
    lua_pushcclosure(f.interpreter(), cancel_timer_closure, 1); // ... [closure]
    luabind::object cancel_func(luabind::from_stack(f.interpreter(), -1)); // ... [closure]

    if(!player->dying)
      timer->start(f);

    return cancel_func;
  }
}

int uptime(lua_player* player)
{
  int uptime_ = (boost::posix_time::microsec_clock::universal_time() - player->start_time)
    .total_milliseconds();
  return uptime_;
}

int register_(lua_State* L)
{
  try
  {
    lua_player* player = luabind::object_cast<lua_player*>(luabind::registry(L)["lua_player"]);
    try
    {
      if(lua_gettop(L) < 1)
      {
        lua_pushstring(L, "Not enough arguments");
        lua_error(L);
      }
      else if(lua_gettop(L) >= 1)
      {
        int i = 1;
        int pos = -1;
        luabind::object first(luabind::from_stack(L, i));
        luabind::object handler;
        if(luabind::type(first) == LUA_TFUNCTION)
          handler = first;
        else if(luabind::type(first) == LUA_TNUMBER && lua_gettop(L) == 1)
        {
          lua_pushstring(L, "Missing handler function argument");
          lua_error(L);
        }
        else if(luabind::type(first) == LUA_TNUMBER)
        {
          pos = luabind::object_cast<int>(first);
          handler = luabind::object(luabind::from_stack(L, ++i));
          if(luabind::type(handler) != LUA_TFUNCTION)
          {
            lua_pushstring(L, "Missing handler function argument (must be first or second argument)");
            lua_error(L);
          }
        }
        else
        {
          lua_pushstring(L, "First argument must be a position or a handler function)");
          lua_error(L);
        }

        std::string class_;
        ++i;
        if(lua_gettop(L) >= i)
        {
          luabind::object class_obj(luabind::from_stack(L, i));
          if(luabind::type(class_obj) != LUA_TSTRING)
          {
            lua_pushstring(L, "You must specifiy the event class before the filters");
            lua_error(L);
          }
          else
            class_ = luabind::object_cast<std::string>(class_obj);
        }

        std::vector<luabind::object> filters;
        for(++i;i <= lua_gettop(L);++i)
        {
          filters.push_back(luabind::object(luabind::from_stack(L, i)));
        }

        cxx_register(L, pos, handler, class_, filters, player);
      }
    }
    catch(luabind::error const& e)
    {
      std::string error = "Error running function event.register: ";
      if(lua_gettop(e.state()) != 0)
        error += lua_tostring(e.state(), -1);
      std::cerr << error << std::endl;
    }
    catch(std::exception const& e)
    {
      std::string error = "Error running function event.register: ";
      error += e.what();
      std::cerr << error << std::endl;
    }
  }
  catch(std::exception& e)
  {
    std::cerr << "Error while registering: " << e.what() << std::endl;
    std::cerr << "Aborting..." << std::endl;
    std::abort(); // TODO Is this OK?
  }

  return 0;
}

} } } } }
