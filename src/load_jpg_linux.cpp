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

#include <ghtv/opengl/linux/load_jpg.hpp>

#include <jpeglib.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace ghtv { namespace opengl { namespace linux_ {

//
//  TODO proper error handling
//

void load_jpg_to_rgba(binary_file const& file, std::vector<unsigned char>& data, std::size_t& width, std::size_t& height)
{
  // Load JPEG file
  binary_file temp_file = file;
  temp_file.load_content();
  std::vector<char> const& c = temp_file.file_content();

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  // Set up decompression structure and error stream.
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  // Set input memory buffer
  jpeg_mem_src(&cinfo, (unsigned char*)&c[0], c.size());

  // Read JPEG header
  jpeg_read_header(&cinfo, TRUE);

  // Set the decompression color space to RGB
  cinfo.out_color_space = JCS_EXT_RGBA;

  width = cinfo.image_width;
  height = cinfo.image_height;
  std::size_t stride = 4u * width;

  data.resize(stride*height);
  unsigned char* data_it = &data[0];

  std::vector<unsigned char> row(stride);
  JSAMPROW row_pointer[1] = { &row[0] };

  // Reading scan lines
  jpeg_start_decompress(&cinfo);
  while(cinfo.output_scanline < height)
  {
    jpeg_read_scanlines(&cinfo, row_pointer, 1);
    std::memcpy(data_it, row_pointer[0], stride);
    data_it += stride;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
}

texture load_jpg(binary_file const& file)
{
  std::cout << "load_jpg " << file.source_uri() << std::endl;

  std::vector<unsigned char> data;
  std::size_t width = 0;
  std::size_t height = 0;

  load_jpg_to_rgba(file, data, width, height);

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
