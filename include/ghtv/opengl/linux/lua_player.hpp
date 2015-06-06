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

#ifndef GHTV_OPENGL_LINUX_LUA_PLAYER_HPP
#define GHTV_OPENGL_LINUX_LUA_PLAYER_HPP

#ifdef GHTV_USE_GLUT
#include <GL/glut.h>
#endif

# ifdef GHTV_USE_OPENGLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include <ghtv/opengl/player_base.hpp>
#include <ghtv/opengl/linux/binary_file.hpp>
#include <ghtv/opengl/linux/lua/event_handler.hpp>
#include <ghtv/opengl/texture.hpp>
#include <luabind/object.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/variant.hpp>
#include <boost/filesystem/path.hpp>
#include <string>
#include <vector>
#include <deque>
#include <map>

struct lua_State;

namespace ghtv { namespace opengl { namespace linux_ {

namespace lua {
struct timer;
struct canvas;
struct socket;
}


struct lua_player : ghtv::opengl::player_base
{
public:
  struct lua_thread
  {
    lua_State* thread;
  };
  struct lua_input_event
  {
    std::string class_str;
    std::map<std::string, std::string> strs;
    std::map<std::string, int> ints;

    static lua_input_event key_event(std::string const& key, bool pressed)
    {
      lua_input_event e;
      e.class_str = "key";
      e.strs["type"] = pressed ? "press" : "release";
      e.strs["key"] = key;
      return e;
    }

    static lua_input_event tcp_event_connect(std::string const& host, int port, int connection_id, std::string err_msg = "")
    {
      lua_input_event e;
      e.class_str = "tcp";
      e.strs["type"] = "connect";
      e.strs["host"] = "host";
      e.ints["port"] = port;
      if(err_msg.empty()) {
        e.ints["connection"] = connection_id;
      } else {
        e.strs["error"] = err_msg;
      }
      return e;
    }

    static lua_input_event tcp_event_data(int connection_id, std::string const& value, std::string err_msg = "")
    {
      lua_input_event e;
      e.class_str = "tcp";
      e.strs["type"] = "data";
      e.ints["connection"] = connection_id;
      if(err_msg.empty()) {
        e.strs["value"] = value;
      } else {
        e.strs["error"] = err_msg;
      }
      return e;
    }
  };
  struct start_area_event
  {
    std::string name;
  };
  struct commit_set_property_event
  {
    std::string name;
    std::string value;
  };
  typedef boost::variant<lua_thread, lua_input_event, start_area_event, commit_set_property_event> execution_variant;

  lua_player(std::string const& identifier, binary_file const& lua_file, std::size_t width, std::size_t height);
  ~lua_player();

  // Basic player interface
  bool has_texture() const { return true; }
  void get_textures(ghtv::opengl::texture*& textures, unsigned& size);
  void key_process(std::string const& key, bool pressed);
  void start_area(std::string const& name) { /*TODO*/ }
  void start();
  void pause() { /*TODO ???*/ }
  void resume() { /*TODO ???*/ }
  bool set_property(std::string const& name, std::string const& value) { /*TODO*/ return false; }
  bool want_keys() const { return true; }


  binary_file get_require_file(std::string const& file);
  binary_file get_canvas_file(std::string const& file);

  lua_State* create_lua_thread();
  void load_lua_file(lua_State* thread, binary_file file);
  void add_lua_thread_to_execution_queue(lua_State* thread);
  void add_lua_file_to_execution_queue(binary_file const& file);
  void add_lua_input_event_to_execution_queue(lua_input_event const& evt);

  static std::string get_lua_error(lua_State* thread);

  /// Lock the "sockets_mutex" before calling this function.
  void create_socket(std::string const& host, int port);
  /// Lock the "sockets_mutex" before calling this function.
  void delete_socket(int connection_id);

  lua_State* L;

  std::string identifier;
  binary_file lua_file;
  std::string lua_file_folder;

  lua::canvas* root_canvas;
  boost::atomic_bool root_canvas_dirty;
  std::size_t width;
  std::size_t height;
  ghtv::opengl::texture texture_;

  boost::atomic_bool dying;

  boost::thread lua_player_thread;

  std::deque<execution_variant> execution_queue;
  boost::mutex execution_queue_mutex;
  boost::condition_variable execution_queue_update_condition;

  typedef std::map<int, lua::socket*> socket_cont;
  int socket_connection_id_generator;
  socket_cont sockets;
  boost::mutex sockets_mutex;

  boost::mutex timers_mutex;
  std::vector<lua::timer*> timers;

  boost::posix_time::ptime start_time;

  std::vector<lua::event_handler> event_handlers;

private:
  void run_global_scope();
  void write_error(std::string const& msg);

  void init_canvas();
  void init_sandbox();
  void init_event();

  void process_lua_thread(lua_State* thread);
  void process_lua_input_event(lua_input_event* evt);
  void process_start_area_event(std::string const& name) { } // TODO
  void process_commit_set_property_events(std::string const& name, std::string const& value) { } // TODO
};

} } }

#endif
