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

#ifndef LUA_CANVAS_HPP
#define LUA_CANVAS_HPP

#include <opencv2/core/core.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <string>
#include <iostream>

struct lua_State;

namespace ghtv { namespace opengl { namespace linux_ {

struct lua_player;

namespace lua {

typedef uchar color_channel;

struct canvas
{
  boost::shared_ptr<std::vector<color_channel> >  rgba;
  cv::Mat mat;
  std::size_t width, height, stride;
  std::size_t scale_width, scale_height;
  int rotation;
  uchar opacity;
  bool flip_x, flip_y;
  std::size_t clip_x, clip_y, clip_w, clip_h;
  std::size_t crop_x, crop_y, crop_w, crop_h;
  color_channel color_red, color_green, color_blue, color_alpha;

  lua_player* player;
  boost::shared_ptr<boost::mutex>  rgba_buffer_mutex;

  canvas()
    : rgba(new std::vector<color_channel>())
    , mat()
    , width(0), height(0), stride(0)
    , scale_width(0), scale_height(0)
    , rotation(0)
    , opacity(255u)
    , flip_x(false), flip_y(false)
    , clip_x(0), clip_y(0), clip_w(0), clip_h(0)
    , crop_x(0), crop_y(0), crop_w(0), crop_h(0)
    , color_red(0u), color_green(0u), color_blue(0u), color_alpha(255u)
    , player(0)
    , rgba_buffer_mutex(new boost::mutex)
  {  }

  canvas(std::size_t w, std::size_t h, lua_player* player = 0)
    : rgba(new std::vector<color_channel>(4u*w*h))
    , mat(h, w, CV_8UC4, &(*rgba)[0])
    , width(w), height(h), stride(4u*w)
    , scale_width(0), scale_height(0)
    , rotation(0)
    , opacity(255u)
    , flip_x(false), flip_y(false)
    , clip_x(0), clip_y(0), clip_w(w), clip_h(h)
    , crop_x(0), crop_y(0), crop_w(w), crop_h(h)
    , color_red(0u), color_green(0u), color_blue(0u), color_alpha(255u)
    , player(player)
    , rgba_buffer_mutex(new boost::mutex)
  {  }

  // NOTE: Relying on default copy constructor and assignment operator

  color_channel* data()
  {
    return mat.data;
  }

  canvas canvas_image_new (std::string image_path);
  canvas canvas_buffer_new (int w, int h);

  void attrSize(int& w, int& h)
  {
    w = width;
    h = height;
  }

  void attrColor(int red, int green, int blue, int alpha);
  void attrColor_string(std::string color)
  {
    if(color == "white")
      attrColor(255u, 255u, 255u, 255u);
    else if(color == "black")
      attrColor(0u, 0u, 0u, 255u);
    else if(color == "silver")
      attrColor(192u,192u,192u, 255u);
    else if(color == "gray")
      attrColor(128u,128u,128u, 255u);
    else if(color == "red")
      attrColor(255u,0u,0u, 255u);
    else if(color == "maroon")
      attrColor(128u,0u,0u, 255u);
    else if(color == "fuchsia")
      attrColor(255u,0u,255u, 255u);
    else if(color == "purple")
      attrColor(128u,0u,128u, 255u);
    else if(color == "lime")
      attrColor(0u,255u,0u, 255u);
    else if(color == "green")
      attrColor(0u,128u,0u, 255u);
    else if(color == "yellow")
      attrColor(255u,255u,0u, 255u);
    else if(color == "olive")
      attrColor(128u,128u,0u, 255u);
    else if(color == "blue")
      attrColor(0u,0u,255u, 255u);
    else if(color == "navy")
      attrColor(0u,0u,128u, 255u);
    else if(color == "aqua")
      attrColor(0u,255u,255u, 255u);
    else if(color == "teal")
      attrColor(0u,128u,128u, 255u);
    else
      std::cerr << "Error: attrColor() unknown color \"" << color << "\"\n";
  }
  void get_attrColor(int& red, int& green, int& blue, int& alpha)
  {
    red = color_red; green = color_green; blue = color_blue; alpha = color_alpha;
  }

  void attrFont(std::string face, int size, std::string style);
  void attrFont_non_normative(std::string face, int size);
  void get_attrFont(lua_State* L);

  void attrClip(int x, int y, int w, int h);
  void get_attrClip(int& x, int& y, int& w, int& h)
  {
    x = clip_x; y = clip_y; w = clip_w; h = clip_h;
  }

  void attrCrop (int x, int y, int w, int h);
  void get_attrCrop (int& x, int& y, int& w, int& h)
  {
    x = crop_x; y = crop_y; w = crop_w; h = crop_h;
  }

  void attrFlip(bool h, bool v)
  {
    if(player) {
      std::cerr << "Can not change opacity attribute of the root canvas." << std::endl;
      return;
    }

    flip_x = h;
    flip_y = v;
  }
  void get_attrFlip(bool& h, bool& v)
  {
    h = flip_x ;
    v = flip_y;
  }

  void attrOpacity(int alpha)
  {
    if(player) {
      std::cerr << "Can not change opacity attribute of the root canvas." << std::endl;
      return;
    }

    opacity = alpha;
  }
  int get_attrOpacity()
  {
    return opacity;
  }

  void attrRotation(int r);
  int get_attrRotation()
  {
    return rotation;
  }

  void attrScale(int w, int h);
  // NOTE: Assuming automatic scale to either "true" or "false"
  void attrScale(bool w, int h){ attrScale(0, h); }
  void attrScale(int w, bool h){ attrScale(w, 0); }
  void attrScale(bool w, bool h){ attrScale(0, 0); }
  void get_attrScale(int& w, int& h)
  {
    w = scale_width;
    h = scale_height;
  }

  void drawLine(int x1, int y1, int x2, int y2)
  {
    // TODO:
  }
  void drawRect(std::string mode, int x, int y, int w, int h);
  void drawRect_fill(int x, int y, int w, int h);
  void drawRoundRect(std::string mode, int x, int y, int w, int h, int arc_w, int arc_h)
  {
    // TODO:
  }
  // should return a C-function
  void drawPolygon(std::string mode)
  {
    // TODO:
  }
  void drawEllipse(std::string mode, int xc, int xy, int w, int h, int ang_s, int ang_e)
  {
    // TODO:
  }

  void drawText(int x, int y, std::string text);

  void clear(int x, int y, int w, int h);
  void clear_default();

  void compose(int x, int y, canvas src);
  void compose(int x, int y, canvas src, int src_x, int srx_y, int src_w, int src_h);
  void compose_impl(int x, int y, canvas src, int src_x, int src_y, int src_w, int src_h, bool has_src_sub_cut);

  void pixel(int x, int y, int red, int green, int blue, int alpha)
  {
    // TODO
  }
  void get_pixel(int& red, int& green, int& blue, int& alpha, int x, int y)
  {
    // TODO:
  }

  void measureText(int& dx, int& dy, std::string text);

  void flush();
};

} } } }


#endif // LUA_CANVAS_HPP
