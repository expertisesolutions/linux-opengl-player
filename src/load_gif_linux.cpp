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

#include <boost/concept_check.hpp>

#define GL_GLEXT_PROTOTYPES

#ifdef GHTV_USE_GLUT
#include <GL/glut.h>
#endif
#ifdef GHTV_USE_OPENGLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <ghtv/opengl/linux/load_gif.hpp>
#include <ghtv/opengl/linux/binary_file.hpp>

#include <gif_lib.h>

#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cassert>

namespace ghtv { namespace opengl { namespace linux_ {

namespace {

struct GifFile
{
  GifFile(binary_file const& file)
    : m_handle()
    , m_binary_file(file)
    , m_mem_stream()
  {
    m_binary_file.load_content();
    m_mem_stream = &(m_binary_file.file_content()[0]);

#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
    int error_code = 0;
    m_handle = DGifOpen(this, gif_input_func, &error_code);
#else
    m_handle = DGifOpen(this, gif_input_func);
#endif
    if(!m_handle) {
      throw std::runtime_error("Could not open " + m_binary_file.source_uri());
    }
  }

  ~GifFile()
  {
    if(!close())
    {
      std::cerr << "Error while closing the gif file." << std::endl;
    }
  }

  bool close()
  {
    bool ret = true;
    if(m_handle)
    {
      ret = (DGifCloseFile(m_handle) == GIF_OK);
      m_handle = 0;
    }
    return ret;
  }

  operator GifFileType*()
  {
    return m_handle;
  }

  GifFileType* operator->()
  {
    return m_handle;
  }

  int read(GifByteType* bytes, int size)
  {
    std::memcpy(bytes, m_mem_stream, size);
    m_mem_stream += size;
    return size;
  }

  static
  int gif_input_func(GifFileType* gif, GifByteType* bytes, int size)
  {
    GifFile* file = (GifFile*)gif->UserData;
    return file->read(bytes, size);
  }

private:
  GifFileType* m_handle;
  binary_file m_binary_file;
  char const* m_mem_stream;
};

void amend_errors_or_throw(GifFileType* gif_file)
{
  if(gif_file->SColorMap && gif_file->SBackGroundColor > gif_file->SColorMap->ColorCount)
  {
    std::cerr << "Gif background color has an invalid value. Setting to the first color." << std::endl;
    gif_file->SBackGroundColor = 0;
  }
}

void get_screen_from_first_image(
  GifFileType* gif_file
, std::vector<unsigned char>& screen_pixels)
{
  GifWord swidth = gif_file->SWidth;
  GifWord sheight = gif_file->SHeight;
  std::size_t ssize = (std::size_t)(swidth * sheight);

  screen_pixels.clear();

  if(swidth == 0 || sheight == 0)
  {
    std::cerr << "Zero pixels gif screen... Nothing to do..." << std::endl;
    return;
  }

  // Allocate pixels
  screen_pixels.resize(ssize);

  // Creating matrix to read the gif pixels
  std::vector<GifRowType> row_vec(sheight);
  GifRowType* screen_buffer = &row_vec[0];
  for(int y = 0; y < sheight; ++y)
  {
    screen_buffer[y] = &screen_pixels[0] + (y * swidth);
  }

  GifRecordType record = TERMINATE_RECORD_TYPE;
  bool got_image = false;

  do {
    if(DGifGetRecordType(gif_file, &record) == GIF_ERROR) {
      throw std::runtime_error("Could not parse gif file");
    }
    switch (record){
    case IMAGE_DESC_RECORD_TYPE:
    {
      // TODO this function does useless work... optmize it?
      if(DGifGetImageDesc(gif_file) == GIF_ERROR)
      {
        throw std::runtime_error("Could not parse gif file");
      }

      GifWord Row = gif_file->Image.Top;
      GifWord Col = gif_file->Image.Left;
      GifWord Width = gif_file->Image.Width;
      GifWord Height = gif_file->Image.Height;

      std::cout << "Image at (" << Col << "," << Row << ") [" << Width << "x" << Height << "]" <<  std::endl;

      if(Col + Width > swidth || Col < 0 ||
         Row + Height > sheight || Row < 0)
      {
        throw std::runtime_error("Image is not confined to screen dimension");
      }

      if(Width == 0 || Height == 0)
      {
        std::cerr << "Zero pixels gif frame... Returning to paint background..." << std::endl;
        return;
      }

      // TODO Shouldn't this be handled by the giflib?
      if(gif_file->Image.Interlace)
      {
        const int InterlacedOffset[] = { 0, 4, 2, 1 }; // The way Interlaced image should
        const int InterlacedJumps[] = { 8, 8, 4, 2 };  // be read - offsets and jumps...
        // Need to perform 4 passes on the images:
        for(int i = 0; i < 4; ++i)
        {
          for(int y = Row + InterlacedOffset[i]; y < Row + Height; y += InterlacedJumps[i])
          {
            if(DGifGetLine(gif_file, &screen_buffer[y][Col], Width) == GIF_ERROR)
            {
              throw std::runtime_error("Could not parse gif file");
            }
          }
        }
      }
      else
      {
        int Row_it = Row;
        for(int y = 0; y < Height; ++y)
        {
          if (DGifGetLine(gif_file, &screen_buffer[Row_it++][Col], Width) == GIF_ERROR)
          {
            throw std::runtime_error("Could not parse gif file");
          }
        }
      }
      got_image = true;
      break;
    }
    case EXTENSION_RECORD_TYPE:
    {
      // TODO Transparent index
      // TODO Plain text image???
      /* Skip any extension blocks in file: */
      int ExtCode;
      GifByteType *Extension;

      // TODO Probaly unnecessary memory allocations are being done here. Check it!!!
      if (DGifGetExtension(gif_file, &ExtCode, &Extension) == GIF_ERROR)
      {
        throw std::runtime_error("Could not parse gif file");
      }

      // TODO Here too!!!
      while (Extension != NULL)
      {
        if (DGifGetExtensionNext(gif_file, &Extension) == GIF_ERROR)
        {
          throw std::runtime_error("Could not parse gif file");
        }
      }
      break;
    }
    case TERMINATE_RECORD_TYPE:
      break;
    default:
      break;
    }
  } while(!got_image && record != TERMINATE_RECORD_TYPE);

  if(!got_image)
  {
    std::cerr << "Gif file does not contain any valid images. Nothing to do..." << std::endl;
  }
}

void screen_to_rgba(
  GifFileType const* gif_file
, std::vector<unsigned char> const& screen_pixels
, std::vector<unsigned char>& rgba_out)
{
  ColorMapObject *color_map = ( gif_file->Image.ColorMap ? gif_file->Image.ColorMap
                              : gif_file->SColorMap);
  // TODO create a default pallete
  if(!color_map)
  {
    throw std::runtime_error("gif image does not contain a color pallete");
  }

  GifWord swidth = gif_file->SWidth;
  GifWord sheight = gif_file->SHeight;

  GifWord left = gif_file->Image.Left;
  GifWord top = gif_file->Image.Top;
  GifWord width = gif_file->Image.Width;
  GifWord height = gif_file->Image.Height;

  // GIF89a Specification does not cover what to set the background to when
  // there is no global pallete.
  // Setting the background to transparency.

  // Creating RGBA buffer and filling it with colors from the gif pallete
  rgba_out.assign(4u * screen_pixels.size(),  0);

  // TODO render all images in the canvas (for "picture wall" like images)
  // TODO Pixel aspect ratio
  // TODO transparent index
  for(int y = top; y < top+height; ++y)
  {
    unsigned char* rgba_row = &rgba_out[0] + (y * 4u * swidth);
    unsigned char const* screen_row = &screen_pixels[0] + (y * swidth);
    for(int x = left; x < left+width; ++x)
    {
      GifColorType* ColorMapEntry = &color_map->Colors[screen_row[x]];

      unsigned char* rgba_pixel = rgba_row + (4u * x);
      rgba_pixel[0] = ColorMapEntry->Red;
      rgba_pixel[1] = ColorMapEntry->Green;
      rgba_pixel[2] = ColorMapEntry->Blue;
      rgba_pixel[3] = 0xff; // 100% opaque pixel
    }
  }

  // Filling background pixels with the global pallete color
  if(gif_file->SColorMap &&
    (left != 0 || top != 0 || width != swidth || height != sheight)
  ){
    // TODO Optmize it!!!
    // It is visiting unnecessary pixels and doing many comparisons.
    GifColorType* bg_color = &gif_file->SColorMap->Colors[gif_file->SBackGroundColor];
    for(int y = 0; y < sheight; ++y)
    {
      unsigned char* rgba_row = &rgba_out[0] + (y * 4u * swidth);
      for(int x = 0; x < swidth; ++x)
      {
        if(y >= top && y < top+height && x >= left && x < left+width)
          continue;

        unsigned char* rgba_pixel = rgba_row + (4u * x);
        rgba_pixel[0] = bg_color->Red;
        rgba_pixel[1] = bg_color->Green;
        rgba_pixel[2] = bg_color->Blue;
        rgba_pixel[3] = 0xff; // 100% opaque pixel
      }
    }
  }
}

} // end of anonymous namespace

void load_gif_to_rgba(binary_file const& file, std::vector<unsigned char>& data, std::size_t& width, std::size_t& height)
{
  GifFile gif_file(file);

  amend_errors_or_throw(gif_file);

  std::vector<unsigned char> screen_pixels;
  get_screen_from_first_image(gif_file, screen_pixels);

  if(screen_pixels.size() ==  0)
  {
    return;
  }

  screen_to_rgba(gif_file, screen_pixels, data);

  width = (std::size_t)(gif_file->SWidth);
  height = (std::size_t)(gif_file->SHeight);

  // TODO Is it OK to close the file without reading it entirely?
  if(!gif_file.close())
  {
    throw std::runtime_error("Could not close gif file");
  }
}


texture load_gif(binary_file const& file)
{
  std::cout << "load_gif " << file.source_uri() << std::endl;

  std::vector<unsigned char> data;
  std::size_t width = 0;
  std::size_t height = 0;

  opengl::texture texture;
  load_gif_to_rgba(file, data, width, height);
  if(data.size() == 0)
    return texture;

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
