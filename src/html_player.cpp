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

#include <ghtv/opengl/linux/html_player.hpp>
#include <ghtv/opengl/texture.hpp>

#include <ghtv/opengl/linux/url_join.hpp>
#include <ghtv/opengl/linux/load_image.hpp>
#include <ghtv/opengl/linux/image_conversion.hpp>

#include <litehtml.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <X11/Xlib.h>
#include <iostream>
#include <cctype>
#include <cmath>


namespace ghtv { namespace opengl { namespace linux_ {

namespace {

struct html_font
{
  cairo_font_face_t*  font;
  int                 size;
  bool                underline;
  bool                strikeout;
};

} //end of anonymous namespace

struct html_player::html_player_impl : public litehtml::document_container
{
public:
  typedef std::map<std::string, opengl::texture> images_map;

  binary_file m_html_file;
  std::size_t m_width;
  std::size_t m_height;
  int m_x;
  int m_y;
  litehtml::position m_client_clip;
  litehtml::position::vector m_clips;

  litehtml::context m_html_context;
  std::string m_base_url;

  int m_screen_width_px;
  int m_screen_height_px;
  double m_screen_ppi;
  double m_screen_pixels_per_point;

  images_map m_images;

  cairo_surface_t* m_html_cr_surface;
  cairo_t* m_html_cr;

  cairo_surface_t* m_temp_cairo_surface;
  cairo_t* m_temp_cairo_cr;

  std::vector<opengl::texture> m_textures;

  /// @warning The litehtml::document destructor calls methods from the
  /// litehtml::document_container (in this case the html_player_impl).
  /// So we are putting it last to ensure that it will be the first destructor
  /// to be called.
  litehtml::document::ptr m_html_doc;

  // TODO More safety checks on the results of external library functions

  // TODO Using this class as both the litehtml::document_container implementation
  // and the drawing handle. Is it OK?

  html_player_impl(binary_file const& file, std::size_t width, std::size_t height)
    : m_html_file(file)
    , m_width(width)
    , m_height(height)
    , m_x(0)
    , m_y(0)
    , m_client_clip()
    , m_clips()
    , m_html_context()
    , m_base_url(m_html_file.url())
    , m_screen_width_px(0)
    , m_screen_height_px(0)
    , m_screen_ppi(96.0) // Defaulting to the common 96 DPI
    , m_screen_pixels_per_point(m_screen_ppi/72.0)
    , m_images()
    , m_html_cr_surface(0)
    , m_html_cr(0)
    , m_temp_cairo_surface(0)
    , m_temp_cairo_cr(0)
    , m_textures()
    , m_html_doc()
  {
    // TODO Grant UTF-8 encoding
    m_html_file.load_content_as_c_str();

    binary_file css_master(binary_file::system_file_t(), "master.css");
    css_master.load_content_as_c_str();
    m_html_context.load_master_stylesheet(css_master.file_content_p());

    {
      Display* display = XOpenDisplay(NULL);
      int screen = 0;
      m_screen_width_px = XDisplayWidth(display, screen);
      m_screen_height_px = XDisplayHeight(display, screen);
// #ifdef GHTV_OPENGL_LINUX_HTML_PLAYER_CALC_SCREEN_PPI
      // 1 inch = 25.4 millimetres
      m_screen_ppi = m_screen_width_px * 25.4 / XDisplayWidthMM(display, screen);
      // 1 point = 1/72 inch
      m_screen_pixels_per_point = m_screen_ppi / 72.0;
// #endif
      XCloseDisplay(display);
    }

    m_temp_cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 2);
    m_temp_cairo_cr = cairo_create(m_temp_cairo_surface);

    cairo_status_t cr_st = cairo_status(m_temp_cairo_cr);
    if(cr_st != CAIRO_STATUS_SUCCESS)
    {
      clean_cairo_data();
      throw std::runtime_error(cairo_status_to_string(cr_st));
    }
  }

  virtual ~html_player_impl()
  {
    clean_cairo_data();
  }

  void clean_cairo_data()
  {
    if(m_temp_cairo_cr)
      cairo_destroy(m_temp_cairo_cr);

    if(m_temp_cairo_surface)
      cairo_surface_destroy(m_temp_cairo_surface);

    if(m_html_cr)
      cairo_destroy(m_html_cr);

    if(m_html_cr_surface)
      cairo_surface_destroy(m_html_cr_surface);
  }

  void parse_and_draw()
  {
    // TODO: Document this code
    m_textures.push_back(opengl::texture(0, 0, m_width, m_height));

    m_html_cr_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, m_width, m_height);
    m_html_cr = cairo_create(m_html_cr_surface);
    cairo_status_t cr_st = cairo_status(m_html_cr);
    if(cr_st != CAIRO_STATUS_SUCCESS)
    {
      throw std::runtime_error(cairo_status_to_string(cr_st));
    }

    cairo_save(m_html_cr);
    {
      cairo_set_source_rgb(m_html_cr, 1.0, 1.0, 1.0);
      cairo_paint(m_html_cr);
    }
    cairo_restore(m_html_cr);

    m_client_clip = litehtml::position(m_x, m_y, m_width, m_height);

    m_html_doc = litehtml::document::createFromString(m_html_file.file_content_p(), this, &m_html_context, 0);
    m_html_doc->render(m_width);
    m_html_doc->draw(this, -m_x, -m_y, &m_client_clip);
  }

  void get_textures(texture*& textures, unsigned& size)
  {
    size = m_textures.size();
    if(!size)
      return;

    // TODO: Dirty flag?
    // TODO: Optimize for desktop OpenGL?

    cairo_surface_flush(m_html_cr_surface);
    std::vector<unsigned char> rgba_data;
    rgba_from_cairo_ARGB32(
      cairo_image_surface_get_data(m_html_cr_surface)
      , m_width
      , m_height
      , cairo_image_surface_get_stride(m_html_cr_surface)
      , rgba_data
    );

    glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 );
    m_textures.front().bind();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(rgba_data[0]));

    assert(glGetError() == GL_NO_ERROR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    assert(glGetError() == GL_NO_ERROR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    assert(glGetError() == GL_NO_ERROR);

    textures = &(m_textures[0]);
  }

  std::string make_url_str(std::string const& basepath, std::string const& url)
  {
    return url_join(!basepath.empty() ? basepath : m_base_url, url);
  }

  std::string make_url_c_str(char const* basepath, char const* url)
  {
    if(!url) {
      throw std::runtime_error(std::string(__func__) + ": NULL url source.");
    }

    if(basepath && basepath[0] != '\0')
    {
      return url_join(basepath, url);
    }
    else
    {
      return url_join(m_base_url, url);
    }
  }

  void apply_clip(cairo_t* cr)
  {
    for(litehtml::position::vector::iterator iter = m_clips.begin(); iter != m_clips.end(); ++iter)
    {
      cairo_rectangle(cr, iter->x, iter->y, iter->width, iter->height);
      cairo_clip(cr);
    }
  }

  void set_color(cairo_t* cr, litehtml::web_color const& color)
  {
    cairo_set_source_rgba(cr, color.red / 255.0, color.green / 255.0, color.blue / 255.0, color.alpha / 255.0);
  }

  void rounded_rectangle(cairo_t* cr, litehtml::position const& pos, litehtml::css_border_radius const& radius)
  {
    cairo_new_path(cr);
    if(radius.top_left_x.val())
    {
      cairo_arc(cr, pos.left() + radius.top_left_x.val(), pos.top() + radius.top_left_x.val(), radius.top_left_x.val(), M_PI, M_PI * 3.0 / 2.0);
    }
    else
    {
      cairo_move_to(cr, pos.left(), pos.top());
    }

    cairo_line_to(cr, pos.right() - radius.top_right_x.val(), pos.top());

    if(radius.top_right_x.val())
    {
      cairo_arc(cr, pos.right() - radius.top_right_x.val(), pos.top() + radius.top_right_x.val(), radius.top_right_x.val(), M_PI * 3.0 / 2.0, 2.0 * M_PI);
    }

    cairo_line_to(cr, pos.right(), pos.bottom() - radius.bottom_right_x.val());

    if(radius.bottom_right_x.val())
    {
      cairo_arc(cr, pos.right() - radius.bottom_right_x.val(), pos.bottom() - radius.bottom_right_x.val(), radius.bottom_right_x.val(), 0, M_PI / 2.0);
    }

    cairo_line_to(cr, pos.left() - radius.bottom_left_x.val(), pos.bottom());

    if(radius.bottom_left_x.val())
    {
      cairo_arc(cr, pos.left() + radius.bottom_left_x.val(), pos.bottom() - radius.bottom_left_x.val(), radius.bottom_left_x.val(), M_PI / 2.0, M_PI);
    }
  }

  void add_path_arc(cairo_t* cr, double x, double y, double rx, double ry, double a1, double a2, bool neg)
  {
    if(rx > 0 && ry > 0)
    {
      cairo_save(cr);

      cairo_translate(cr, x, y);
      cairo_scale(cr, 1, ry / rx);
      cairo_translate(cr, -x, -y);

      if(neg)
      {
        cairo_arc_negative(cr, x, y, rx, a1, a2);
      }
      else
      {
        cairo_arc(cr, x, y, rx, a1, a2);
      }

      cairo_restore(cr);
    }
    else
    {
      cairo_move_to(cr, x, y);
    }
  }

  //
  // Begin of the public callback interface of the litehtml::document_container
  // {
public:

  virtual litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm)
  {
    if(!fm)
    {
      std::cerr << __func__ << ": font metrics not requested" << std::endl;
      return 0;
    }

    litehtml::string_vector fonts;
    litehtml::tokenize(faceName, fonts, ",");
    litehtml::trim(fonts[0]);

    cairo_font_face_t* fnt = 0;

    FcPattern* pattern = FcPatternCreate();
    bool found = false;
    for(litehtml::string_vector::iterator i = fonts.begin(); i != fonts.end(); ++i)
    {
      if(FcPatternAddString(pattern, FC_FAMILY, (unsigned char*) i->c_str()))
      {
        found = true;
        break;
      }
    }
    if(found)
    {
      if(italic == litehtml::fontStyleItalic )
      {
        FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
      }
      else
      {
        FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
      }

      int fc_weight = FC_WEIGHT_NORMAL;
      if(weight >= 0 && weight < 150)         fc_weight = FC_WEIGHT_THIN;
      else if(weight >= 150 && weight < 250)  fc_weight = FC_WEIGHT_EXTRALIGHT;
      else if(weight >= 250 && weight < 350)  fc_weight = FC_WEIGHT_LIGHT;
      else if(weight >= 350 && weight < 450)  fc_weight = FC_WEIGHT_NORMAL;
      else if(weight >= 450 && weight < 550)  fc_weight = FC_WEIGHT_MEDIUM;
      else if(weight >= 550 && weight < 650)  fc_weight = FC_WEIGHT_SEMIBOLD;
      else if(weight >= 650 && weight < 750)  fc_weight = FC_WEIGHT_BOLD;
      else if(weight >= 750 && weight < 850)  fc_weight = FC_WEIGHT_EXTRABOLD;
      else if(weight >= 850 && weight < 950)  fc_weight = FC_WEIGHT_BLACK;
      else if(weight >= 950)                  fc_weight = FC_WEIGHT_EXTRABLACK;

      FcPatternAddInteger (pattern, FC_WEIGHT, fc_weight);

      fnt = cairo_ft_font_face_create_for_pattern(pattern);
    }

    FcPatternDestroy(pattern);

    if(!fnt)
    {
      std::cerr << __func__ << ": could not create font face \"" << faceName << "\"" << std::endl;
      return 0;
    }

    cairo_save(m_temp_cairo_cr);
    {
      cairo_set_font_face(m_temp_cairo_cr, fnt);
      cairo_set_font_size(m_temp_cairo_cr, size);
      cairo_font_extents_t ext;
      cairo_font_extents(m_temp_cairo_cr, &ext);

      cairo_text_extents_t tex;
      cairo_text_extents(m_temp_cairo_cr, "x", &tex);

      fm->ascent      = (int) ext.ascent;
      fm->descent     = (int) ext.descent;
      fm->height      = (int) (ext.ascent + ext.descent);
      fm->x_height    = (int) tex.height;
    }
    cairo_restore(m_temp_cairo_cr);

    html_font* ret  = new html_font;
    ret->font       = fnt;
    ret->size       = size;
    ret->strikeout  = (decoration & litehtml::font_decoration_linethrough) ? true : false;
    ret->underline  = (decoration & litehtml::font_decoration_underline) ? true : false;

    return (litehtml::uint_ptr) ret;
  }

  virtual void delete_font(litehtml::uint_ptr hFont)
  {
    html_font* fnt = (html_font*) hFont;
    if(fnt)
    {
        cairo_font_face_destroy(fnt->font);
        delete fnt;
    }
  }

  virtual int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont)
  {
    html_font* fnt = (html_font*) hFont;

    cairo_save(m_temp_cairo_cr);

    cairo_set_font_size(m_temp_cairo_cr, fnt->size);
    cairo_set_font_face(m_temp_cairo_cr, fnt->font);
    cairo_text_extents_t ext;
    cairo_text_extents(m_temp_cairo_cr, text, &ext);

    cairo_restore(m_temp_cairo_cr);

    return (int) ext.x_advance;
  }

  virtual void draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos)
  {
    // TODO: Use Pango ???
    html_font* fnt = (html_font*) hFont;
    cairo_t* cr     = m_html_cr;
    cairo_save(cr);

    apply_clip(cr);

    cairo_set_font_face(cr, fnt->font);
    cairo_set_font_size(cr, fnt->size);
    cairo_font_extents_t ext;
    cairo_font_extents(cr, &ext);

    int x = pos.left();
    int y = pos.bottom() - ext.descent;

    set_color(cr, color);

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);

    int tw = 0;

    if(fnt->underline || fnt->strikeout)
    {
      tw = text_width(text, hFont);
    }

    if(fnt->underline)
    {
      cairo_set_line_width(cr, 1);
      cairo_move_to(cr, x, y + 1.5);
      cairo_line_to(cr, x + tw, y + 1.5);
      cairo_stroke(cr);
    }

    if(fnt->strikeout)
    {
      cairo_text_extents_t tex;
      cairo_text_extents(cr, "x", &tex);

      int ln_y = y - tex.height / 2.0;

      cairo_set_line_width(cr, 1);
      cairo_move_to(cr, x, (double) ln_y - 0.5);
      cairo_line_to(cr, x + tw, (double) ln_y - 0.5);
      cairo_stroke(cr);
    }

    cairo_restore(cr);
  }

  virtual int pt_to_px(int pt)
  {
    return (int)(pt * m_screen_pixels_per_point + 0.5);
  }

  virtual int get_default_font_size()
  {
    return 16;
  }

  virtual const litehtml::tchar_t* get_default_font_name()
  {
    return "serif";
  }

  virtual void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker)
  {
    // TODO
  }

  virtual void load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready)
  {
    try
    {
      std::string image_url = make_url_c_str(baseurl, src);
      if(m_images.find(image_url) == m_images.end())
      {
        binary_file image_file(m_html_file.root(), m_base_url, image_url);
        opengl::texture t = opengl::linux_::load_image(image_file);
        m_images[image_url] = t;
      }
    }
    catch(std::exception& e)
    {
      std::cerr << e.what() << std::endl;
    }
  }

  virtual void get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz)
  {
    sz.width = 0;
    sz.height = 0;

    try
    {
      images_map::iterator img = m_images.find(make_url_c_str(baseurl, src));
      if(img != m_images.end())
      {
        sz.width = img->second.width;
        sz.height = img->second.height;
      }
    }
    catch(std::exception& e)
    {
      std::cerr << e.what() << std::endl;
    }
  }

  virtual void draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg)
  {
    cairo_t* cr = m_html_cr;
    cairo_save(cr);
    {
      apply_clip(cr);

      rounded_rectangle(cr, bg.border_box, bg.border_radius);
      cairo_clip(cr);

      cairo_rectangle(cr, bg.clip_box.x, bg.clip_box.y, bg.clip_box.width, bg.clip_box.height);
      cairo_clip(cr);

      if(bg.color.alpha)
      {
        set_color(cr, bg.color);
        cairo_paint(cr);
      }
    }
    cairo_restore(cr);

    // Handling images separately for optimization reasons
    std::string image_url = make_url_str(bg.baseurl, bg.image);
    images_map::iterator img = m_images.find(image_url);
    if(img != m_images.end())
    {
      opengl::texture t = img->second;
      t.set_x(bg.position_x);
      t.set_y(bg.position_y);
      t.set_width(bg.image_size.width);
      t.set_height(bg.image_size.height);

      if(t.x < m_x)
      {
        t.set_clip_left(m_x - t.x);
      }

      if(t.x + t.width > m_x + m_width)
      {
        t.set_clip_right(m_x + m_width - t.x);
      }

      if(t.y < m_y)
      {
        t.set_clip_top(m_y - t.y);
      }

      if(t.y + t.height > m_y + m_height)
      {
        t.set_clip_bottom(m_y + m_height - t.y);
      }

      m_textures.push_back(t);
    }
  }

  virtual void draw_borders(litehtml::uint_ptr hdc, const litehtml::css_borders& borders, const litehtml::position& draw_pos, bool root )
  {
    cairo_t* cr = m_html_cr;
    cairo_save(cr);
    apply_clip(cr);

    cairo_new_path(cr);

    int bdr_top     = 0;
    int bdr_bottom  = 0;
    int bdr_left    = 0;
    int bdr_right   = 0;

    if(borders.top.width.val() != 0 && borders.top.style > litehtml::border_style_hidden)
    {
      bdr_top = (int) borders.top.width.val();
    }
    if(borders.bottom.width.val() != 0 && borders.bottom.style > litehtml::border_style_hidden)
    {
      bdr_bottom = (int) borders.bottom.width.val();
    }
    if(borders.left.width.val() != 0 && borders.left.style > litehtml::border_style_hidden)
    {
      bdr_left = (int) borders.left.width.val();
    }
    if(borders.right.width.val() != 0 && borders.right.style > litehtml::border_style_hidden)
    {
      bdr_right = (int) borders.right.width.val();
    }

    // draw right border
    if(bdr_right)
    {
      set_color(cr, borders.right.color);

      double r_top    = borders.radius.top_right_x.val();
      double r_bottom = borders.radius.bottom_right_x.val();

      if(r_top)
      {
        double end_angle    = 2 * M_PI;
        double start_angle  = end_angle - M_PI / 2.0  / ((double) bdr_top / (double) bdr_right + 1);
        add_path_arc(cr
          , draw_pos.right() - r_top, draw_pos.top() + r_top
          , r_top - bdr_right, r_top - bdr_right + (bdr_right - bdr_top)
          , end_angle, start_angle, true);
        add_path_arc(cr
          , draw_pos.right() - r_top, draw_pos.top() + r_top
          , r_top, r_top
          , start_angle, end_angle, false);
      }
      else
      {
        cairo_move_to(cr, draw_pos.right() - bdr_right, draw_pos.top() + bdr_top);
        cairo_line_to(cr, draw_pos.right(), draw_pos.top());
      }

      if(r_bottom)
      {
        cairo_line_to(cr, draw_pos.right(), draw_pos.bottom() - r_bottom);
        double start_angle  = 0;
        double end_angle    = start_angle + M_PI / 2.0  / ((double) bdr_bottom / (double) bdr_right + 1);
        add_path_arc(cr,
          draw_pos.right() - r_bottom, draw_pos.bottom() - r_bottom,
          r_bottom, r_bottom,
          start_angle, end_angle, false);
        add_path_arc(cr,
          draw_pos.right() - r_bottom, draw_pos.bottom() - r_bottom,
          r_bottom - bdr_right, r_bottom - bdr_right + (bdr_right - bdr_bottom),
          end_angle, start_angle, true);
      }
      else
      {
        cairo_line_to(cr, draw_pos.right(), draw_pos.bottom());
        cairo_line_to(cr, draw_pos.right() - bdr_right, draw_pos.bottom() - bdr_bottom);
      }

      cairo_fill(cr);
    }

    // draw bottom border
    if(bdr_bottom)
    {
      set_color(cr, borders.bottom.color);

      double r_left   = borders.radius.bottom_left_x.val();
      double r_right  = borders.radius.bottom_right_x.val();

      if(r_left)
      {
        double start_angle  = M_PI / 2.0;
        double end_angle    = start_angle + M_PI / 2.0  / ((double) bdr_left / (double) bdr_bottom + 1);

        add_path_arc(cr,
          draw_pos.left() + r_left, draw_pos.bottom() - r_left,
          r_left - bdr_bottom + (bdr_bottom - bdr_left), r_left - bdr_bottom,
          start_angle, end_angle, false);

        add_path_arc(cr,
            draw_pos.left() + r_left, draw_pos.bottom() - r_left,
            r_left, r_left,
            end_angle, start_angle, true);
      }
      else
      {
        cairo_move_to(cr, draw_pos.left(), draw_pos.bottom());
        cairo_line_to(cr, draw_pos.left() + bdr_left, draw_pos.bottom() - bdr_bottom);
      }

      if(r_right)
      {
        cairo_line_to(cr, draw_pos.right() - r_right,   draw_pos.bottom());

        double end_angle    = M_PI / 2.0;
        double start_angle  = end_angle - M_PI / 2.0  / ((double) bdr_right / (double) bdr_bottom + 1);

        add_path_arc(cr,
          draw_pos.right() - r_right, draw_pos.bottom() - r_right,
          r_right, r_right,
          end_angle, start_angle, true);

        add_path_arc(cr,
          draw_pos.right() - r_right, draw_pos.bottom() - r_right,
          r_right - bdr_bottom + (bdr_bottom - bdr_right), r_right - bdr_bottom,
          start_angle, end_angle, false);
      }
      else
      {
        cairo_line_to(cr, draw_pos.right() - bdr_right, draw_pos.bottom() - bdr_bottom);
        cairo_line_to(cr, draw_pos.right(), draw_pos.bottom());
      }

      cairo_fill(cr);
    }

    // draw top border
    if(bdr_top)
    {
      set_color(cr, borders.top.color);

      double r_left   = borders.radius.top_left_x.val();
      double r_right  = borders.radius.top_right_x.val();

      if(r_left)
      {
        double end_angle    = M_PI * 3.0 / 2.0;
        double start_angle  = end_angle - M_PI / 2.0  / ((double) bdr_left / (double) bdr_top + 1);

        add_path_arc(cr,
          draw_pos.left() + r_left, draw_pos.top() + r_left,
          r_left, r_left,
          end_angle, start_angle, true);

        add_path_arc(cr,
          draw_pos.left() + r_left, draw_pos.top() + r_left,
          r_left - bdr_top + (bdr_top - bdr_left), r_left - bdr_top,
          start_angle, end_angle, false);
      }
      else
      {
        cairo_move_to(cr, draw_pos.left(), draw_pos.top());
        cairo_line_to(cr, draw_pos.left() + bdr_left, draw_pos.top() + bdr_top);
      }

      if(r_right)
      {
        cairo_line_to(cr, draw_pos.right() - r_right,   draw_pos.top() + bdr_top);

        double start_angle  = M_PI * 3.0 / 2.0;
        double end_angle    = start_angle + M_PI / 2.0  / ((double) bdr_right / (double) bdr_top + 1);

        add_path_arc(cr,
          draw_pos.right() - r_right, draw_pos.top() + r_right,
          r_right - bdr_top + (bdr_top - bdr_right), r_right - bdr_top,
          start_angle, end_angle, false);

        add_path_arc(cr,
          draw_pos.right() - r_right, draw_pos.top() + r_right,
          r_right, r_right,
          end_angle, start_angle, true);
      }
      else
      {
        cairo_line_to(cr, draw_pos.right() - bdr_right, draw_pos.top() + bdr_top);
        cairo_line_to(cr, draw_pos.right(), draw_pos.top());
      }

      cairo_fill(cr);
    }

    // draw left border
    if(bdr_left)
    {
      set_color(cr, borders.left.color);

      double r_top    = borders.radius.top_left_x.val();
      double r_bottom = borders.radius.bottom_left_x.val();

      if(r_top)
      {
        double start_angle  = M_PI;
        double end_angle    = start_angle + M_PI / 2.0  / ((double) bdr_top / (double) bdr_left + 1);

        add_path_arc(cr,
          draw_pos.left() + r_top, draw_pos.top() + r_top,
          r_top - bdr_left, r_top - bdr_left + (bdr_left - bdr_top),
          start_angle, end_angle, false);

        add_path_arc(cr,
          draw_pos.left() + r_top, draw_pos.top() + r_top,
          r_top, r_top,
          end_angle, start_angle, true);
      }
      else
      {
        cairo_move_to(cr, draw_pos.left() + bdr_left, draw_pos.top() + bdr_top);
        cairo_line_to(cr, draw_pos.left(), draw_pos.top());
      }

      if(r_bottom)
      {
        cairo_line_to(cr, draw_pos.left(),  draw_pos.bottom() - r_bottom);

        double end_angle    = M_PI;
        double start_angle  = end_angle - M_PI / 2.0  / ((double) bdr_bottom / (double) bdr_left + 1);

        add_path_arc(cr,
          draw_pos.left() + r_bottom, draw_pos.bottom() - r_bottom,
          r_bottom, r_bottom,
          end_angle, start_angle, true);

        add_path_arc(cr,
          draw_pos.left() + r_bottom, draw_pos.bottom() - r_bottom,
          r_bottom - bdr_left, r_bottom - bdr_left + (bdr_left - bdr_bottom),
          start_angle, end_angle, false);
      }
      else
      {
        cairo_line_to(cr, draw_pos.left(),  draw_pos.bottom());
        cairo_line_to(cr, draw_pos.left() + bdr_left,   draw_pos.bottom() - bdr_bottom);
      }

      cairo_fill(cr);
    }
    cairo_restore(cr);
  }

  virtual void set_caption(const litehtml::tchar_t* caption)
  {
  }

  virtual void set_base_url(const litehtml::tchar_t* base_url)
  {
    if(base_url)
    {
      std::cout << "base_url:" << base_url << std::endl;
      try
      {
        m_base_url = make_url_str(m_html_file.url(), base_url);

        // NOTE m_base_url should never be empty
        if(!m_base_url.empty()) {
          return; // Everything OK. Returning.
        }
      }
      catch(std::exception& e)
      {
        std::cerr << e.what() << std::endl;
      }
    }

    // In case of any error default to the page address
    m_base_url = m_html_file.url();
  }

  virtual void link(litehtml::document* doc, litehtml::element::ptr el)
  {
  }

  virtual void on_anchor_click(const litehtml::tchar_t* url, litehtml::element::ptr el)
  {
  }

  virtual void set_cursor(const litehtml::tchar_t* cursor)
  {
  }

  virtual void transform_text(litehtml::tstring& text, litehtml::text_transform tt)
  {
    // TODO Unicode transformations
  }

  virtual void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl)
  {
    text.clear();
    try
    {
      binary_file css_file(m_html_file.root(), !baseurl.empty() ? baseurl : m_base_url, url);
      css_file.load_content_as_c_str();
      if(css_file.file_content().size() > 1)
      {
        text = css_file.file_content_p();
        // TODO: Why this???
        baseurl = css_file.url();
      }
    }
    catch(std::exception& e)
    {
      std::cerr << e.what() << std::endl;
    }
  }

  virtual void set_clip(const litehtml::position& pos, bool valid_x, bool valid_y)
  {
    litehtml::position clip_pos = pos;
    if(!valid_x)
    {
      clip_pos.x      = 0;
      clip_pos.width  = m_width;
    }
    if(!valid_y)
    {
      clip_pos.y      = 0;
      clip_pos.height = m_height;
    }
    m_clips.push_back(clip_pos);
  }

  virtual void del_clip()
  {
    if(!m_clips.empty())
    {
      m_clips.pop_back();
    }
  }

  virtual void get_client_rect(litehtml::position& client)
  {
    client.x = 0;
    client.y = 0;
    client.width = m_width;
    client.height = m_height;
  }

  virtual litehtml::element* create_element(const litehtml::tchar_t* tag_name)
  {
    // TODO: Implement forms
    return 0;
  }

  virtual void get_media_features(litehtml::media_features& media)
  {
    media.type          = litehtml::media_type_tv;
    media.width         = m_width;
    media.height        = m_height;
    media.device_width  = m_screen_width_px;
    media.device_height = m_screen_height_px;
    media.color         = 8;
    media.monochrome    = 0;
    media.color_index   = 256;
    media.resolution    = (int)(m_screen_ppi + 0.5);
  }

  // }
  // End of the public callback interface of the litehtml::document_container
  //
};

html_player::html_player(binary_file const& file, std::size_t width, std::size_t height)
  : impl(new html_player_impl(file, width, height))
{  }

html_player::~html_player()
{
  delete impl;
}

void html_player::start()
{
  impl->parse_and_draw();
}

void html_player::get_textures(opengl::texture*& textures, unsigned& size)
{
  impl->get_textures(textures, size);
}

} } }
