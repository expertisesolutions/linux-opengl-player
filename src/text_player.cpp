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

#include <ghtv/opengl/linux/text_player.hpp>
#include <ghtv/opengl/linux/image_conversion.hpp>
#include <ghtv/opengl/texture.hpp>

#include <pango/pangocairo.h>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <cassert>

namespace ghtv { namespace opengl { namespace linux_ {

namespace
{

void set_color(unsigned char* c, unsigned char r, unsigned char g, unsigned char b)
{
  c[0] = r;
  c[1] = g;
  c[2] = b;
}

} // end of anonimous namespace

struct text_player::text_player_impl
{
  // Member variables
  struct font_properties
  {
    std::string Family;
    PangoStyle Style;
    PangoWeight Weight;
    PangoVariant Variant;
    std::string ColorName;
    unsigned char ColorRGB[3];
    int Size;
  } m_font;

  binary_file m_text_file;
  std::size_t m_width;
  std::size_t m_height;

  opengl::texture m_texture;

  text_player_impl(binary_file const& text_file_arg, std::size_t width, std::size_t height)
    : m_text_file(text_file_arg)
    , m_width(width)
    , m_height(height)
    , m_texture(0, 0, width, height)
  {
    // Set default values
    m_font.Family = "Tiresias";
    m_font.Style = PANGO_STYLE_NORMAL;
    m_font.Weight = PANGO_WEIGHT_NORMAL;
    m_font.Variant = PANGO_VARIANT_NORMAL;

    m_font.ColorName = "white";
    m_font.ColorRGB[0] = 255u;
    m_font.ColorRGB[1] = 255u;
    m_font.ColorRGB[2] = 255u;

    m_font.Size = 16;

    m_text_file.load_content_as_c_str();
  }

  bool set_property(std::string const& name, std::string const& value)
  {
    if(name == "fontFamily")
    {
      m_font.Family = value;
    }
    else if(name == "fontSize")
    {
      m_font.Size = boost::lexical_cast<int>(value);
    }
    else if(name == "fontStyle")
    {
      if(value == "normal") {
        m_font.Style = PANGO_STYLE_NORMAL;
      } else if(value == "italic") {
        m_font.Style = PANGO_STYLE_ITALIC;
      } else {
        std::cerr << "text_player error: unknown fontStyle \"" << value << "\"\n";
        return false;
      }
    }
    else if(name == "fontWeight")
    {
      if(value == "normal") {
        m_font.Weight = PANGO_WEIGHT_NORMAL;
      } else if(value == "bold") {
        m_font.Weight = PANGO_WEIGHT_BOLD;
      } else {
        std::cerr << "text_player error: unknown fontWeight \"" << value << "\"\n";
        return false;
      }
    }
    else if(name == "fontVariant")
    {

      if(value == "normal") {
        m_font.Variant = PANGO_VARIANT_NORMAL;
      } else if(value == "small-caps") {
        m_font.Variant = PANGO_VARIANT_SMALL_CAPS;
      } else {
        std::cerr << "text_player error: unknown fontVariant \"" << value << "\"\n";
        return false;
      }
    }
    else if(name == "fontColor")
    {
      unsigned char* c = m_font.ColorRGB; // shortcut
      if(value == "white")
        set_color(c, 255u, 255u, 255u);
      else if(value == "black")
        set_color(c, 0u, 0u, 0u);
      else if(value == "silver")
        set_color(c, 192u,192u,192u);
      else if(value == "gray")
        set_color(c, 128u,128u,128u);
      else if(value == "red")
        set_color(c, 255u,0u,0u);
      else if(value == "maroon")
        set_color(c, 128u,0u,0u);
      else if(value == "fuchsia")
        set_color(c, 255u,0u,255u);
      else if(value == "purple")
        set_color(c, 128u,0u,128u);
      else if(value == "lime")
        set_color(c, 0u,255u,0u);
      else if(value == "green")
        set_color(c, 0u,128u,0u);
      else if(value == "yellow")
        set_color(c, 255u,255u,0u);
      else if(value == "olive")
        set_color(c, 128u,128u,0u);
      else if(value == "blue")
        set_color(c, 0u,0u,255u);
      else if(value == "navy")
        set_color(c, 0u,0u,128u);
      else if(value == "aqua")
        set_color(c, 0u,255u,255u);
      else if(value == "teal")
        set_color(c, 0u,128u,128u);
      else
      {
        std::cerr << "text_player error: unknown fontColor \"" << value << "\"\n";
        return false;
      }

      // Set color name
      m_font.ColorName = value;
    }
    else
    {
      std::cerr << "text_player error: Unknown property name=\"" << name << "\" value=\"" << value << "\"\n";
      return false;
    }
    return true;
  }

  void start()
  {
    cairo_surface_t* cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, m_width, m_height);
    cairo_t* cr = cairo_create(cairo_surface);

    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    PangoFontDescription *font_description = pango_font_description_new();
    pango_font_description_set_family(font_description, m_font.Family.c_str());
    pango_font_description_set_style(font_description, m_font.Style);
    pango_font_description_set_weight(font_description, m_font.Weight);
    pango_font_description_set_variant(font_description, m_font.Variant);
    pango_font_description_set_size(font_description, m_font.Size * PANGO_SCALE);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_description);
    pango_layout_set_text(layout, m_text_file.file_content_p(), -1);

    unsigned char* color = m_font.ColorRGB; // shortcut
    cairo_set_source_rgb(cr, color[0] / 255.0, color[1] / 255.0, color[2] / 255.0);
    cairo_move_to(cr, 0.0, 0.0);
    pango_cairo_show_layout(cr, layout);

    // To texture
    {
      cairo_surface_flush(cairo_surface);
      std::vector<unsigned char> rgba_data;
      rgba_from_cairo_ARGB32(
        cairo_image_surface_get_data(cairo_surface)
        , m_width
        , m_height
        , cairo_image_surface_get_stride(cairo_surface)
        , rgba_data
      );

      glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 );
      m_texture.bind();

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(rgba_data[0]));

      assert(glGetError() == GL_NO_ERROR);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      assert(glGetError() == GL_NO_ERROR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      assert(glGetError() == GL_NO_ERROR);
    }

    g_object_unref(layout);
    pango_font_description_free(font_description);

    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surface);
  }
};

text_player::text_player(binary_file const& text_file_arg, std::size_t width, std::size_t height)
  : impl(0)
{
  impl = new text_player_impl(text_file_arg, width, height);
}

text_player::~text_player()
{
  delete impl;
}

void text_player::get_textures(opengl::texture*& textures, unsigned& size)
{
  textures = &(impl->m_texture);
  size = 1;
}

void text_player::start()
{
  impl->start();
}

bool text_player::set_property(std::string const& name, std::string const& value)
{
  return impl->set_property(name, value);
}

} } }

