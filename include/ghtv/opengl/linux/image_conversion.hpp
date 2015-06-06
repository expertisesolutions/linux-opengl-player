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

#ifndef GHTV_OPENGL_LINUX_IMAGE_CONVERSION_HPP
#define GHTV_OPENGL_LINUX_IMAGE_CONVERSION_HPP

#include <vector>

namespace ghtv { namespace opengl { namespace linux_ {

/// @note The returned buffer has not padding bytes.
inline void rgba_from_cairo_ARGB32(void* data, int width, int height, int stride, std::vector<unsigned char>& output)
{
  const int line_size = 4 * width;
  output.resize(line_size * height);
  unsigned char* dest = &output[0];

  unsigned char* src = (unsigned char*) data;
  unsigned char* src_end = src + (height*stride);

  // if 0x01 is the first byte the machine is little endian
  long multibyte_integer = 1;
  bool is_little_endian = ((char*)&multibyte_integer)[0] != 0;

  while(src != src_end)
  {
    for(unsigned char* px = src; px != src + line_size; px += 4)
    {
      // Cairo ARGB32:
      // * little endian ..... BGRA
      // * big endian ........ ARGB

      // TODO Undo premultiplied alpha??
      if(is_little_endian) {
        *dest++ = px[2];
        *dest++ = px[1];
        *dest++ = px[0];
        *dest++ = px[3];
      } else {
        *dest++ = px[1];
        *dest++ = px[2];
        *dest++ = px[3];
        *dest++ = px[0];
      }
    }
    src += stride;
  }
}

} } }

#endif
