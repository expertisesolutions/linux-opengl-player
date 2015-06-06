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

#ifndef GHTV_OPENGL_LINUX_SOUND_PLAYER_HPP
#define GHTV_OPENGL_LINUX_SOUND_PLAYER_HPP

#include <ghtv/opengl/player_base.hpp>

#include <ghtv/opengl/linux/binary_file.hpp>

#include <string>

namespace ghtv { namespace opengl { namespace linux_ {

struct sound_player : ghtv::opengl::player_base
{
  sound_player(binary_file const& file);
  ~sound_player();

  bool has_texture() const { return false; }
  void get_textures(texture*& textures, unsigned& size) { throw std::runtime_error(std::string(__func__) + ": Not implemented"); }
  void key_process(std::string const& key, bool pressed) {}
  void start_area(std::string const& name) { /*TODO ???*/ }
  void start();
  void pause();
  void resume();
  bool set_property(std::string const& name, std::string const& value);
  bool want_keys() const { return false; }

  struct sound_player_data;
  sound_player_data* m;
};

} } }

#endif
