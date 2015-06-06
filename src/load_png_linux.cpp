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

#include <ghtv/opengl/linux/load_png.hpp>

#include <png.h>

#include <cstdio>
#include <cstring>
#include <vector>
#include <stdexcept>

namespace ghtv { namespace opengl { namespace linux_ {

namespace {

struct png_stream
{
  png_stream(png_bytep file_content)
    : m_mem_stream(file_content)
  {  }

  void read(png_bytep out_bytes, png_size_t size)
  {
    std::memcpy(out_bytes, m_mem_stream, size);
    m_mem_stream += size;
  }

  static
  void read_data_from_stream(png_structp png_ptr, png_bytep out_bytes, png_size_t size)
  {
    png_stream* stream = (png_stream*) png_ptr->io_ptr;
    stream->read(out_bytes, size);
  }

  png_bytep m_mem_stream;
};

} // end of anonymous namespace

void load_png_to_rgba(binary_file const& file, std::vector<unsigned char>& data, std::size_t& width_, std::size_t& height_)
{
  std::cout << "load_png " << file.source_uri() << std::endl;

  binary_file tmp_file = file;
  tmp_file.load_content();
  png_stream stream((png_bytep)&tmp_file.file_content()[0]);

  png_structp png_ptr = png_create_read_struct
    (PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if(!png_ptr) {
    throw std::runtime_error("error: could not start libpng decoder");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
  {
    png_destroy_read_struct(&png_ptr
                            , static_cast<png_infopp>(0)
                            , static_cast<png_infopp>(0));
    throw std::runtime_error("error: could not start libpng decoder");
  }

  png_set_read_fn(png_ptr, &stream, png_stream::read_data_from_stream);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 width = 0;
  png_uint_32 height = 0;
  int bit_depth = 0;
  int color_type = -1;
  int channels = png_get_channels(png_ptr, info_ptr);
  png_uint_32 retval = png_get_IHDR(png_ptr, info_ptr
                                    , &width, &height, &bit_depth, &color_type
                                    , 0, 0, 0);
  if(!retval) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    throw std::runtime_error("Could not read PNG file IHDR.");
  }

//     std::cout << "channels: " << channels << std::endl;
//     std::cout << "bit depth: " << bit_depth << std::endl;
//     std::cout << "color_type: " << color_type << std::endl;

  switch(color_type)
  {
  case PNG_COLOR_TYPE_PALETTE:
    std::cout << "PNG_COLOR_TYPE_PALETTE" << std::endl;
    png_set_palette_to_rgb(png_ptr);
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      png_set_tRNS_to_alpha(png_ptr);
    else
      png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    channels = 4;
    break;
  case PNG_COLOR_TYPE_GRAY_ALPHA:
    png_set_gray_to_rgb(png_ptr);
    channels = 4;
    break;
  case PNG_COLOR_TYPE_GRAY:
    png_set_expand_gray_1_2_4_to_8(png_ptr);
    png_set_gray_to_rgb(png_ptr);
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    bit_depth = 8;
    channels = 4;
    break;
  case PNG_COLOR_TYPE_RGB:
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    channels = 4;
    break;
  case PNG_COLOR_TYPE_RGBA:
    break;
  default:
    // std::cerr << "Color type not implemented yet. " << color_type << std::endl;
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    throw std::runtime_error("PNG color type not implemented yet");
  }

  if(bit_depth == 16) {
    png_set_strip_16(png_ptr);
    bit_depth = 8;
  }

  std::vector<png_bytep> row_ptrs(height);
  const std::size_t stride = width*channels*bit_depth/8;
  data.resize(stride * height);
  png_bytep data_ptr = &data[0];
  for(std::size_t i = 0; i != height; ++i)
  {
    row_ptrs[i] = data_ptr + (i)*stride;
  }

  png_read_image(png_ptr, &row_ptrs[0]);
  width_ = width;
  height_ = height;
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
}

texture load_png(binary_file const& file)
{
  std::vector<unsigned char> data;
  std::size_t width = 0;
  std::size_t height = 0;

  load_png_to_rgba(file, data, width, height);

  opengl::texture texture;

  glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 );
  texture.bind();

  std::cout << "width " << width << " height " << height << std::endl;

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA
                , GL_UNSIGNED_BYTE, &data[0]);

  assert(glGetError() == GL_NO_ERROR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  assert(glGetError() == GL_NO_ERROR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  assert(glGetError() == GL_NO_ERROR);

  return texture;
}


} } }

