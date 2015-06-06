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

#ifdef GHTV_USE_GLUT
#include <GL/glut.h>
#endif

#ifdef GHTV_USE_OPENGLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include <ghtv/opengl/linux/lua_player.hpp>

#include <ghtv/opengl/linux/lua/canvas.hpp>
#include <ghtv/opengl/linux/lua/timer.hpp>
#include <ghtv/opengl/linux/lua/event.hpp>
#include <ghtv/opengl/linux/lua/socket.hpp>

#include <lua.hpp>
#include <luabind/luabind.hpp>
#include <luabind/object.hpp>
#include <luabind/tag_function.hpp>
#include <luabind/out_value_policy.hpp>
#include <luabind/scope.hpp>

#include <boost/filesystem.hpp>

#include <fstream>
#include <vector>
#include <iostream>
#include <cstdio>

namespace ghtv { namespace opengl { namespace linux_ {

// Hook for yielding the execution of lua functions
static void yield_lua_hook(lua_State* l, lua_Debug* ar)
{
//   lua_pushlightuserdata(l, (void*)"lua_player"); // ... [str]
//   lua_gettable(l, LUA_REGISTRYINDEX);            // ... [ptr]
//   lua_player* player = lua_topointer(l, -1);     // ... [ptr]
//   lua_pop(l, 1);                                 // ...

  lua_player* player = luabind::object_cast<lua_player*>(luabind::registry(l)["lua_player"]);
  if(player->dying)
    lua_yield(l, 0);
}


std::string lua_player::get_lua_error(lua_State* thread)
{
  if(lua_gettop(thread) >= 1)
  {
    luabind::object error_msg(luabind::from_stack(thread, -1));
    if(luabind::type(error_msg) == LUA_TSTRING)
      return luabind::object_cast<std::string>(error_msg);
  }
  return "Unknown lua error!!!";
}

lua_player::lua_player(std::string const& identifier, binary_file const& lua_file, std::size_t width, std::size_t height)
  : L(0)
  , identifier(identifier)
  , lua_file(lua_file)
  , lua_file_folder()
  , root_canvas(0)
  , root_canvas_dirty(false)
  , width(width)
  , height(height)
  , texture_(0, 0, width, height)
  , dying(false)
  , socket_connection_id_generator(0)
{
  if(lua_file.is_local()) {
    lua_file_folder = boost::filesystem::path(lua_file.file_posix_path())
                      .remove_filename().string() + "/";
  }

  L = luaL_newstate();
  lua_sethook(L, yield_lua_hook, LUA_MASKCOUNT, 512); // TODO try diferent values

  try
  {
    luaL_openlibs(L);

    luabind::open(L);

    luabind::module(L, "ghtv")
    [
      luabind::class_<lua_player>("lua_player")
    ];

    luabind::registry(L)["lua_player"] = this;

    init_sandbox();
    init_canvas();
    init_event();
  }
  catch(std::exception const& e)
  {
    write_error(e.what());
    throw;
  }
}

lua_player::~lua_player()
{
  dying = true;

  // Warning! Try not to use interruption points inside lua callbacks, since
  // this call to lua_player_thread.interrupt() may make them throw an
  // exception when the player is destroyed.
  lua_player_thread.interrupt();
  lua_player_thread.join();

  // Once we get the "timers_mutex" the timers can no longer delete themselves.
  {
    boost::lock_guard<boost::mutex> lock(timers_mutex);
    for(std::vector<lua::timer*>::iterator it = timers.begin(); it != timers.end(); ++it)
    {
      (*it)->timer_thread.interrupt();
    }
  }

  for(std::vector<lua::timer*>::iterator it = timers.begin(); it != timers.end(); ++it)
  {
    (*it)->timer_thread.join();
    delete *it;
  }

  // Once we get the "sockets_mutex" the sockets can no longer delete themselves.
  {
    boost::lock_guard<boost::mutex> lock(sockets_mutex);
    for(socket_cont::iterator it = sockets.begin(); it != sockets.end(); ++it)
    {
      it->second->interrupt();
      it->second->stop();
    }
  }

  for(socket_cont::iterator it = sockets.begin(); it != sockets.end(); ++it)
  {
    it->second->join();
    delete it->second;
  }

  // Should destroy these objects before closing the lua state.
  // It avoids the access of deallocated memory by the destructors of the
  // luabind objects.
  timers.clear();
  execution_queue.clear();
  event_handlers.clear();
  sockets.clear();

  lua_close(L);

  std::cout << "lua_player destroyed" << std::endl;
}

void lua_player::start()
{
  try
  {
    start_time = boost::posix_time::microsec_clock::universal_time();
    add_lua_file_to_execution_queue(lua_file);
    lua_player_thread = boost::thread(&lua_player::run_global_scope, this);
  }
  catch(std::exception const& e)
  {
    write_error(e.what());
    throw;
  }
}

void lua_player::get_textures(ghtv::opengl::texture*& textures, unsigned& size)
{
  textures = &texture_;
  size = 1;

  if(!root_canvas_dirty) {
    // std::cout << "root_canvas not updated!" << std::endl;
    return;
  }

  {
    boost::lock_guard<boost::mutex> bitmap_lock(*(root_canvas->rgba_buffer_mutex));

    root_canvas_dirty = false;

    // Does not change... already set on construction
    // texture_.width = root_canvas->width;
    // texture_.height = root_canvas->height;

    glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 );
    texture_.bind();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, root_canvas->width, root_canvas->height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, root_canvas->data());

    assert(glGetError() == GL_NO_ERROR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    assert(glGetError() == GL_NO_ERROR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    assert(glGetError() == GL_NO_ERROR);
  }
}


void lua_player::key_process(std::string const& key, bool pressed)
{
  add_lua_input_event_to_execution_queue(lua_input_event::key_event(key, pressed));
}

void lua_player::write_error(std::string const& msg)
{
  std::string error = "Error while proccessing file ";
  error += lua_file.source_uri();
  error += " with error: ";
  error += msg;
  std::cerr << error << std::endl;
}

binary_file lua_player::get_require_file(std::string const& name)
{
  if(lua_file.is_local()) {
    return binary_file(lua_file.root(), lua_file_folder, name + ".lua");
  }

  // TODO :If the lua_file is not local the require is relative to the
  // NCL root folder or to the website?
  return binary_file(lua_file.root(), name + ".lua");
}

binary_file lua_player::get_canvas_file(std::string const& file)
{
  // TODO: Optimize: avoid root revalidation (checked root object?)
  return binary_file(lua_file.root(), lua_file_folder, file);
}

lua_State* lua_player::create_lua_thread()
{
#ifndef NDEBUG
  int old_top = lua_gettop(L);
#endif
  lua_State* thread = lua_newthread(L); // ... [thread_lua]

  // Adding to the LUA_REGISTRYINDEX to avoid invalid memory access when
  // timers expire.
  lua_pushlightuserdata(L, thread);     // ... [thread_lua] [thread_ptr]
  lua_insert(L, -2);                    // ... [thread_ptr] [thread_lua]
  // registry[thread_ptr] = thread_lua
  lua_settable(L, LUA_REGISTRYINDEX);   // ...

  assert(old_top == lua_gettop(L));
  return thread;
}

void lua_player::create_socket(std::string const& host, int port)
{
  ++socket_connection_id_generator;
  lua::socket* socket = new lua::socket(this, host, port, socket_connection_id_generator);
  sockets[socket_connection_id_generator] = socket;
  socket->start();
}

void lua_player::delete_socket(int connection_id)
{
  socket_cont::iterator it = sockets.find(connection_id);
  assert(it != sockets.end());
  delete it->second;
  sockets.erase(it);
}

void lua_player::load_lua_file(lua_State* thread, binary_file file)
{
  file.load_content();
  std::vector<char> const& c = file.file_content();
  // TODO block if bytecode
  int r = luaL_loadbuffer(thread, &c[0], c.size(), file.source_uri().c_str());
  file.release_content();
  if (r !=  0)
  {
    throw std::runtime_error(lua_player::get_lua_error(thread).c_str());
  }
}

void lua_player::add_lua_file_to_execution_queue(binary_file const& file)
{
  lua_State* thread = create_lua_thread();
  load_lua_file(thread, file);
  add_lua_thread_to_execution_queue(thread);
}

void lua_player::add_lua_thread_to_execution_queue(lua_State* thread)
{
  {
    boost::lock_guard<boost::mutex> lock(execution_queue_mutex);
    lua_thread t = {thread};
    execution_queue.push_back(t);
  }
  execution_queue_update_condition.notify_all();
}

void lua_player::add_lua_input_event_to_execution_queue(lua_input_event const& evt)
{
  {
    boost::lock_guard<boost::mutex> lock(execution_queue_mutex);
    execution_queue.push_back(evt);
  }
  execution_queue_update_condition.notify_all();
}

void lua_player::process_lua_thread(lua_State* thread)
{
  int r = lua_resume(thread, 0);
  if(r == LUA_YIELD) {
    std::cout << "Lua thread yield with " << lua_gettop(thread) << " arguments." << std::endl;
  } else if(r != 0) {
    throw std::runtime_error(lua_player::get_lua_error(thread).c_str());
  }
}

void lua_player::process_lua_input_event(lua_input_event* evt)
{
  assert(!!evt);
#ifndef NDEBUG
  int old_top = lua_gettop(L);
#endif
  for(std::vector<lua::event_handler>::const_iterator first = event_handlers.begin()
        , last = event_handlers.end(); first != last; ++first)
  {
    if(!first->class_.empty() && first->class_ != evt->class_str)
    {
      continue;
    }

    boost::this_thread::interruption_point();

    try
    {
      assert(luabind::type(first->handler) == LUA_TFUNCTION);

      lua_State* thread = create_lua_thread();

  #ifndef NDEBUG
      int old_top = lua_gettop(thread);
  #endif

      first->handler.push(first->handler.interpreter());
      lua_xmove(first->handler.interpreter(), thread, 1); // ... [function]

      lua_newtable(thread);                               // ... [function] [table]

      lua_pushstring(thread, "class");                    // ... [function] [table] ["class"]
      lua_pushstring(thread, evt->class_str.c_str());     // ... [function] [table] ["class"] ["class_str"]
      // table["class"] = "key"
      lua_settable(thread, -3);                           // ... [function] [table]

      for(std::map<std::string, std::string>::const_iterator
          it = evt->strs.begin(), end = evt->strs.end();
          it != end; ++it)
      {
        lua_pushstring(thread, it->first.c_str());  // ... [function] [table] ["key"]
        lua_pushstring(thread, it->second.c_str()); // ... [function] [table] ["key"] ["value"]
        // table["key"] = "value"
        lua_settable(thread, -3);                   // ... [function] [table]
      }

      for(std::map<std::string, int>::const_iterator
          it = evt->ints.begin(), end = evt->ints.end();
          it != end; ++it)
      {
        lua_pushstring(thread, it->first.c_str()); // ... [function] [table] ["key"]
        lua_pushinteger(thread, it->second);       // ... [function] [table] ["key"] [value]
        // table["key"] = value
        lua_settable(thread, -3);                  // ... [function] [table]
      }

      assert(old_top + 2 == lua_gettop(thread));

      int r = lua_resume(thread, 1);
      if(r == LUA_YIELD)
      {
        std::cout << "Lua thread yield with " << lua_gettop(thread) << " arguments." << std::endl;
        return;
      }

      if(r != 0) {
        throw std::runtime_error(lua_player::get_lua_error(thread).c_str());
      }

      // std::cout << "Key " << key << " handled" << std::endl;

      if(lua_gettop(thread) >= 1) {
        int ret = lua_toboolean(thread, -1);
        lua_pop(thread, 1);
        if(ret) {
          break;
        }
      }

      assert(old_top == lua_gettop(thread));
    }
    catch(luabind::error const& e)
    {
      std::string error = "Error running handler for \"" + evt->class_str + "\" event: ";
      if(lua_gettop(e.state()) != 0)
        error += lua_tostring(e.state(), -1);
      std::cerr << error << std::endl;
    }
    catch(std::exception const& e)
    {
      std::string error = "Error running handler for key: ";
      error += e.what();
      std::cerr << error << std::endl;
    }
  }
  assert(lua_gettop(L) == old_top);
}

void lua_player::run_global_scope()
{
  try
  {
    // Execution loop
    for(;;)
    {
      execution_variant v;
      {
        boost::unique_lock<boost::mutex> lock(execution_queue_mutex);
        boost::this_thread::interruption_point();
        while(execution_queue.empty())
        {
          //std::cout << "No more lua threads to execute. Waiting..." << std::endl;
          execution_queue_update_condition.wait(lock);
        }
        v = execution_queue.front();
        execution_queue.pop_front();
      }

      if(lua_thread* e = boost::get<lua_thread>(&v))
      {
        process_lua_thread(e->thread);
      }
      else if(lua_input_event* e = boost::get<lua_input_event>(&v))
      {
        process_lua_input_event(e);
      }
      else if(start_area_event* e = boost::get<start_area_event>(&v))
      {
        process_start_area_event(e->name);
      }
      else if(commit_set_property_event* e = boost::get<commit_set_property_event>(&v))
      {
        process_commit_set_property_events(e->name, e->value);
      }
      boost::this_thread::interruption_point();
    }
  }
  catch (boost::thread_interrupted& interruption)
  {
    std::cout << "Lua player interrupted..." << std::endl;
  }
  catch(std::exception const& e)
  {
    write_error(e.what());
  }
}

} } }
