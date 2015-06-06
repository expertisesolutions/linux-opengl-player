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

#ifndef GHTV_OPENGL_LINUX_GLOBAL_STATE_HPP
#define GHTV_OPENGL_LINUX_GLOBAL_STATE_HPP

#include <ghtv/opengl/main_state.hpp>

#include <gntl/parser/libxml2/dom/xml_document.hpp>
#include <gntl/parser/libxml2/dom/document.hpp>

#ifdef  GHTV_USE_OPENMAX
#include <ghtv/omx-rpi/image_pipeline.hpp>
#endif

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace ghtv { namespace opengl { namespace linux_ {

struct player_factory
{
  typedef boost::shared_ptr<ghtv::opengl::player_base> result_type;
  typedef std::string component_identifier;
  result_type operator()(component_identifier source
                         , gntl::algorithm::structure::media::dimensions dim
                         , boost::filesystem::path root_path
                         , std::string const& identifier) const;
};
      
struct state
{
  typedef ghtv::opengl::main_state<gntl::parser::libxml2::dom::document, player_factory>
    main_state_type;

  main_state_type main_state;

  boost::shared_ptr<gntl::parser::libxml2::dom::xml_document> xml_document;
  GLuint program_object;
  GLint projection_location, z_location, texture_location
    , border_color_location, clip_location;
  int width, height;
#ifdef GHTV_USE_OPENMAX
  boost::mutex pipeline_mutex;
  boost::condition_variable pipeline_condition;

  typedef boost::shared_ptr<omx_rpi::image_pipeline> image_pipeline;
  
  std::vector<image_pipeline> free_image_pipelines;
  std::vector<image_pipeline> reset_image_pipelines;
  std::list<image_pipeline> busy_image_pipelines;

  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLSurface egl_surface;
  EGL_DISPMANX_WINDOW_T nativewindow;
#endif

  state() : width(1280), height(720)
  {}
};

extern state global_state;
      
} } }

#endif
