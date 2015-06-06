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

#ifndef GHTV_OPENGL_LINUX_STATIC_TEXTURE_PLAYER_HPP
#define GHTV_OPENGL_LINUX_STATIC_TEXTURE_PLAYER_HPP

#include <ghtv/opengl/linux/load_png.hpp>
#include <ghtv/opengl/linux/load_jpg.hpp>
#include <ghtv/opengl/linux/load_gif.hpp>

#include <ghtv/opengl/linux/lua_player.hpp>
#include <ghtv/opengl/linux/sound_player.hpp>
#include <ghtv/opengl/linux/html_player.hpp>
#include <ghtv/opengl/linux/text_player.hpp>

#include <ghtv/opengl/player_base.hpp>

#include <boost/algorithm/string.hpp>

namespace ghtv { namespace opengl { namespace linux_ {

struct static_texture_player : ghtv::opengl::player_base
{
  static_texture_player(ghtv::opengl::texture t)
    : texture_(t)
  {
  }


  bool has_texture() const { return true; }

  void get_textures(ghtv::opengl::texture*& textures, unsigned& size)
  {
    textures = &texture_;
    size = 1;
  }

  void key_process(std::string const& key, bool pressed)
  {
    std::cout << "Processing key " << key << std::endl;
  }

  void start_area(std::string const& name) { /*TODO ???*/ }
  void start() {}
  void pause() {}
  void resume() {}
  bool set_property(std::string const& name, std::string const& value) { /*TODO ???*/ return true; }
  bool want_keys() const { return false; }

  ghtv::opengl::texture texture_;
};

struct async_static_texture_player : ghtv::opengl::player_base
{
  async_static_texture_player()
    : busy(true)
  {
  }
  ~async_static_texture_player()
  {
#ifdef GHTV_USE_OPENMAX
    boost::unique_lock<boost::mutex> l(global_state.pipeline_mutex);
    while(busy) global_state.pipeline_condition.wait(l);
#endif
  }

#ifdef GHTV_USE_OPENMAX
  void texture_ready(opengl::texture t, std::list<state::image_pipeline>::iterator pipeline)
  {
    texture_ = t;
    texture_.good = true; // TODO Is it right?

    boost::unique_lock<boost::mutex> l(global_state.pipeline_mutex);
    busy = false;

    global_state.reset_image_pipelines.push_back(*pipeline);
    global_state.busy_image_pipelines.erase(pipeline);
    global_state.pipeline_condition.notify_all();
  }
#endif


  bool has_texture() const { return true; }

  void get_textures(ghtv::opengl::texture*& textures, unsigned& size)
  {
    textures = &texture_;
    size = 1;
  }

  void key_process(std::string const& key, bool pressed)
  {
    std::cout << "Processing key " << key << std::endl;
  }

  void start_area(std::string const& name) { /*TODO ???*/ }
  void start() {}
  void pause() {}
  void resume() {}
  bool set_property(std::string const& name, std::string const& value) { /*TODO ???*/ return true; }
  bool want_keys() const { return false; }

  ghtv::opengl::texture texture_;
  bool busy;
};
      
inline
player_factory::result_type player_factory::operator()
  (component_identifier source
   , gntl::algorithm::structure::media::dimensions dim
   , boost::filesystem::path root_path
   , std::string const& identifier) const
{
  // TODO: Use smart pointers to transfer file objects everywhere
  binary_file file(root_path.string(), source);
  std::string source_str = source;

  // TODO: Get file MIME type ?
  if(boost::algorithm::iends_with(source_str, ".txt"))
  {
    return result_type(
      new ghtv::opengl::linux_::text_player(file, dim.width, dim.height)
    );
  }

  if(boost::algorithm::iends_with(source_str, ".html") ||
     boost::algorithm::iends_with(source_str, ".htm") ||
     boost::algorithm::iends_with(source_str, ".xhtml"))
  {
    return result_type(
      new ghtv::opengl::linux_::html_player(file, dim.width, dim.height)
    );
  }

  if(boost::algorithm::iends_with(source_str, ".mp3") ||
    boost::algorithm::iends_with(source_str, ".mp2") ||
    boost::algorithm::iends_with(source_str, ".wav"))
  {
    return result_type(
      new ghtv::opengl::linux_::sound_player(file)
    );
  }

  if(boost::algorithm::iends_with(source_str, ".lua"))
  {
    return result_type(
      new ghtv::opengl::linux_::lua_player(identifier, file, dim.width, dim.height)
    );
  }

  if(boost::algorithm::iends_with(source_str, ".gif"))
  {
    ghtv::opengl::texture texture = load_gif(file);
    texture.set_width(dim.width);
    texture.set_height(dim.height);
    return result_type(
      new ghtv::opengl::linux_::static_texture_player(texture)
    );
  }

#ifndef GHTV_USE_OPENMAX
  ghtv::opengl::texture texture;
  if(boost::algorithm::iends_with(source_str, ".png"))
  {
    texture = load_png(file);
  }
  else if(boost::algorithm::iends_with(source_str, ".jpeg") || boost::algorithm::iends_with(source_str, ".jpg"))
  {
    texture = load_jpg(file);
  }
  else
  {
    throw std::runtime_error("File type not supported: " + source_str);
  }
  texture.set_width(dim.width);
  texture.set_height(dim.height);
#endif

  result_type p(new ghtv::opengl::linux_
#ifndef GHTV_USE_OPENMAX
                ::static_texture_player(texture)
#else
                ::async_static_texture_player
#endif
                );

#ifdef GHTV_USE_OPENMAX
  if(boost::algorithm::iends_with(source_str, ".png"))
  {
    ghtv::opengl::linux_::load_png
      (file, boost::bind(&async_static_texture_player::texture_ready
                                , static_cast<async_static_texture_player*>(p.get()), _1, _2));
  }
  else// if(boost::algorithm::iends_with(source_str, ".jpeg"))
  {
    throw std::runtime_error("File type not supported: " + source_str);
  }
#endif  
  return p;
}
      
} } }

#endif
