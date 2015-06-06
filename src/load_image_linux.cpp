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

#include <ghtv/opengl/linux/load_image.hpp>

#include <ghtv/opengl/linux/load_gif.hpp>
#include <ghtv/opengl/linux/load_jpg.hpp>
#include <ghtv/opengl/linux/load_png.hpp>

#include <boost/algorithm/string.hpp>

namespace ghtv { namespace opengl { namespace linux_ {

void load_image_to_rgba(binary_file const& file, std::vector<unsigned char>& data, std::size_t& width, std::size_t& height)
{
  std::string file_name = file.source_uri();
  if(boost::algorithm::iends_with(file_name, ".png"))
  {
    load_png_to_rgba(file, data, width, height);
  }
  else if(boost::algorithm::iends_with(file_name, ".jpeg") || boost::algorithm::iends_with(file_name, ".jpg"))
  {
    load_jpg_to_rgba(file, data, width, height);
  }
  else if(boost::algorithm::iends_with(file_name, ".gif"))
  {
    load_gif_to_rgba(file, data, width, height);
  }
  else
  {
    throw std::runtime_error("Could not load image \"" + file.source_uri() + "\": unknown file type");
  }
}

texture load_image(binary_file const& file)
{
  // TODO check file MIME ???
  std::string file_name = file.source_uri();
  if(boost::algorithm::iends_with(file_name, ".png"))
  {
    return load_png(file);
  }
  else if(boost::algorithm::iends_with(file_name, ".jpeg") || boost::algorithm::iends_with(file_name, ".jpg"))
  {
    return load_jpg(file);
  }
  else if(boost::algorithm::iends_with(file_name, ".gif"))
  {
    return load_gif(file);
  }
  else
  {
    throw std::runtime_error("Could not load image \"" + file.source_uri() + "\": unknown file type");
  }
}

} } }
