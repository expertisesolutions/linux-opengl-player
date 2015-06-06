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

#ifndef GHTV_OPENGL_LINUX_LOAD_PNG_OMX_HPP
#define GHTV_OPENGL_LINUX_LOAD_PNG_OMX_HPP

#include <GLES2/gl2.h>

#include <ghtv/opengl/texture.hpp>
#include <ghtv/opengl/linux/global_state.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/bind.hpp>

#include <cstdio>

namespace ghtv { namespace opengl { namespace linux_ {

template <typename F>
void load_png(boost::filesystem::path file_name, F f)
{
  boost::unique_lock<boost::mutex> l(global_state.pipeline_mutex);
  while(global_state.free_image_pipelines.empty()
        && global_state.reset_image_pipelines.empty())
  {
    global_state.pipeline_condition.wait(l);
  }

  boost::shared_ptr<omx_rpi::image_pipeline> pipeline;
  if(!global_state.free_image_pipelines.empty())
  {
    pipeline = global_state.free_image_pipelines.back();

    global_state.free_image_pipelines.pop_back();
  }
  else if(!global_state.reset_image_pipelines.empty())
  {

    pipeline = global_state.reset_image_pipelines.back();

    global_state.reset_image_pipelines.pop_back();
    l.unlock();
    pipeline->reset();

  }
  if(!l.owns_lock())
    l.lock();

  global_state.busy_image_pipelines.push_front(pipeline);
  std::list<state::image_pipeline>::iterator
    pipeline_iterator = global_state.busy_image_pipelines.begin();

  opengl::texture texture;
  l.unlock();
  pipeline->load_image
    (file_name.c_str(), texture.raw(), &global_state.egl_display, &global_state.egl_context
     , boost::bind(f, texture, pipeline_iterator));

}

} } }

#endif
