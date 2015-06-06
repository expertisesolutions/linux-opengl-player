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

#ifndef GHTV_OPENGL_LINUX_TEXT_PLAYER_HPP
#define GHTV_OPENGL_LINUX_TEXT_PLAYER_HPP

#include <ghtv/opengl/player_base.hpp>
#include <ghtv/opengl/linux/binary_file.hpp>

#include <iostream>
#include <string>
#include <map>

namespace ghtv { namespace opengl { namespace linux_ {

struct text_player : ghtv::opengl::player_base
{
  text_player(binary_file const& text_file_arg, std::size_t width, std::size_t height);
  ~text_player();

  bool has_texture() const { return true; }

  void get_textures(opengl::texture*& textures, unsigned& size);

  void key_process(std::string const& key, bool pressed)
  {
    std::cout << __FILE__ ": key_process " << key << std::endl;
  }

  void start_area(std::string const& name) { /*TODO ???*/ }
  void start();
  void pause() {}
  void resume() {}
  bool set_property(std::string const& name, std::string const& value);
  bool want_keys() const { return false; }

private:
  struct text_player_impl;
  text_player_impl* impl;
};

} } }

#endif
