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

#include <ghtv/opengl/linux/lua/canvas.hpp>

#include <ghtv/opengl/linux/lua_player.hpp>
#include <ghtv/opengl/linux/load_image.hpp>
#include <ghtv/opengl/linux/idle_update.hpp>

#include <opencv2/imgproc/imgproc.hpp>
#include <boost/thread/lock_guard.hpp>
#include <lua.hpp>
#include <vector>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace ghtv { namespace opengl { namespace linux_ { namespace lua {

namespace {

int in_limits(int v, int lower, int upper)
{
  return v < lower ? lower :
         v > upper ? upper :
         v;
}

// TODO Is this standard safe? (unsigned char)(255.f + 0.5f) == 0xff
color_channel round_color(float v)
{
  return (color_channel)(v + 0.5f);
}

void alpha_blend(color_channel* dpixel, color_channel const* spixel, float src_opacity)
{
  float sa = src_opacity * spixel[3] / 255.f;
  for(int k = 0; k < 3; ++k) {
    float dk = dpixel[k];
    dpixel[k] = round_color(dk + sa*(spixel[k]-dk));
  }
  dpixel[3] = round_color(dpixel[3] + sa*(0xff-dpixel[3]));
}

void assign_color(color_channel* dpixel, color_channel const* spixel)
{
  for(int k = 0; k < 4; ++k) {
    dpixel[k] = spixel[k];
    // *dpixel++ = *spixel++;
  }
}

} // end of anonymous namespace

canvas canvas::canvas_image_new(std::string image_path)
{
  canvas c;
  if (!player)
  {
    std::cerr << "Error: Can not call \"canvas_image_new\" from a auxiliary canvas." << std::endl;
    return c;
  }

  try
  {
    load_image_to_rgba(player->get_canvas_file(image_path), *(c.rgba), c.width, c.height);
    c.mat = cv::Mat(c.height, c.width, CV_8UC4, &(*(c.rgba))[0]);
    c.stride = 4u * c.width;
    c.clip_w = c.crop_w = c.width;
    c.clip_h = c.crop_h = c.height;
  }
  catch(std::exception& e)
  {
    std::cerr << "Error while loading image \"" << image_path << "\": " << e.what() << std::endl;
  }
  catch(...)
  {
    std::cerr << "Unknown error while loading image \"" << image_path << "\"" << std::endl;
  }
  return c;
}

canvas canvas::canvas_buffer_new (int w, int h)
{

  if (!player)
  {
    std::cerr << "Error: Can not call \"canvas_buffer_new\" from a auxiliary canvas." << std::endl;
    return canvas();
  }

  if(w < 0 || h < 0) {
    std::cerr << "Error! Negative dimensions should not be supplied to \"canvas_buffer_new\"." << std::endl;
    return canvas();
  }
  return canvas(w, h);
}

void canvas::attrColor(int red, int green, int blue, int alpha)
{
  color_red = red; color_green = green; color_blue = blue; color_alpha = alpha;
}

void canvas::attrFont(std::string face, int size, std::string style)
{
  // TODO
}

void canvas::attrFont_non_normative(std::string face, int size)
{
  // TODO
}

void canvas::get_attrFont(lua_State* L)
{
  // TODO
  lua_pushstring(L, "string");
  lua_pushnumber(L, 10);
  lua_pushstring(L, "anotherstring");
}

void canvas::attrClip(int x, int y, int w, int h)
{
  clip_x = in_limits(x, 0, width);
  clip_y = in_limits(y, 0, height);
  clip_w = in_limits(w, 0, width);
  clip_h = in_limits(h, 0, height);
}

void canvas::attrCrop (int x, int y, int w, int h)
{
  if(player)
  {
    std::cerr << "Can not change crop attribute of the root canvas." << std::endl;
    return;
  }
  crop_x = in_limits(x, 0, width);
  crop_y = in_limits(y, 0, height);
  crop_w = in_limits(w, 0, width);
  crop_h = in_limits(h, 0, height);
}

void canvas::attrRotation(int r)
{
  if(player) {
    std::cerr << "Can not change rotation attribute of the root canvas." << std::endl;
    return;
  }

  if(r % 90 != 0) {
    std::cerr << "Error: rotation value should be a multiple of 90." << std::endl;
    return;
  }

  // if(r < 0) r = (r % 360) + 360; // I think that normaly the above will be faster :-)
  while(r < 0) r += 360;

  rotation = (r / 90) % 4;
}

void canvas::attrScale(int w, int h)
{
  if(player)
  {
    std::cerr << "Can not change scale attribute of the root canvas." << std::endl;
    return;
  }

  scale_width = w;
  scale_height = h;
}

void canvas::drawRect(std::string mode, int x, int y, int w, int h)
{
  if(mode == "fill") {
    drawRect_fill(x, y, w, h);
  } else if(mode == "frame") {
    // TODO
    std::cerr << "drawRect frame not implemented." << std::endl;
  } else {
    std::cerr << "Unsupported draw mode: \"" << mode << "\"." << std::endl;
  }
}

void canvas::drawRect_fill(int x, int y, int w, int h)
{
  // std::cout << "x:" << x << " y:" << y << " w:" << w << " h:" << h << std::endl;

  // Assuming these values fits into a long.
  // Theirs actual values came from int parameters, so it should be OK.
  const long left = std::max(x, (int)clip_x);
  const long right = std::min((long)(clip_x+clip_w), (long)std::min(x+w, (int)width));
  const long up = std::max(y, (int)clip_y);
  const long down = std::min((long)(clip_y+clip_h), (long)std::min(y+h, (int)height));

  // std::cout << "left:" << left << " right:" << right << " up:" << up << " down:" << down << std::endl;

  if(left >= right || up >= down)
  {
    // std::cerr << "Nothing to draw..." << std::endl;
    return;
  }

  color_channel color[] = {color_red, color_green, color_blue, color_alpha};
  color_channel* first_row = data() + up*stride + 4u*left;

  std::size_t line_size = (right-left)*4u;

  color_channel* row = first_row;
  color_channel* row_end = first_row + line_size;

  {
    boost::lock_guard<boost::mutex> image_lock(*rgba_buffer_mutex);
    while(row != row_end)
    {
      memcpy(row, color, 4u);
      row += 4u;
    }

    row = first_row + stride;
    row_end = first_row + (down-up)*stride;
    while(row != row_end)
    {
      memcpy(row, first_row, line_size);
      row += stride;
    }

    if(player)
      player->root_canvas_dirty = true;
  }
}

void canvas::clear(int x, int y, int w, int h)
{
  drawRect_fill(x, y, w, h);
}

void canvas::clear_default()
{
  drawRect_fill(0, 0, width, height);
}

// TODO Is clipping in "src" relevant???
void canvas::compose(int x, int y, canvas src)
{
  compose_impl(x, y, src, 0, 0, 0, 0, false);
}

void canvas::compose(int x, int y, canvas src, int src_x, int src_y, int src_w, int src_h)
{
  compose_impl(x, y, src, src_x, src_y, src_w, src_h, true);
}

void canvas::compose_impl(int x, int y, canvas src, int src_x, int src_y, int src_w, int src_h, bool has_src_sub_cut)
{
  src.flush(); // flush to follow the standard

  try
  {
    cv::Mat src_mat( src.mat(cv::Rect(src.crop_x, src.crop_y, src.crop_w, src.crop_h)).clone() );

    if(has_src_sub_cut) {
      src_mat = src_mat(cv::Rect(src_x, src_y, src_w, src_h));
    }

    bool src_flip_x = src.flip_x;
    bool src_flip_y = src.flip_y;
    if(src.rotation != 0)
    {
      if(src.rotation == 2) {
        src_flip_x = !src_flip_x;
        src_flip_y = !src_flip_y;
      } else {
        cv::transpose(src_mat, src_mat);
        if(src.rotation == 1) src_flip_x = !src_flip_x;
        else                  src_flip_y = !src_flip_y;
      }
    }

    // TODO: rotation scale to original size???

    if(src_flip_x || src_flip_y)
    {
      int flip_code = src_flip_y ? src_flip_x ? -1 : 0 : 1;
      cv::flip(src_mat, src_mat, flip_code);
    }

    if(src.scale_width || src.scale_height)
    {
      double fx = 1;
      double fy = 1;
      if(src.scale_width) {
        fx = (double) src.scale_width / src_mat.cols;
        if(!src.scale_height)
          fy = fx;
      }
      if(src.scale_height) {
        fy = (double) src.scale_height / src_mat.rows;
        if(!src.scale_width)
          fx = fy;
      }
      if(fx != 1.0 || fy != 1.0)
        cv::resize(src_mat, src_mat, cv::Size(), fx, fy, cv::INTER_LINEAR);
    }

    cv::Rect draw_area( cv::Rect(clip_x, clip_y, clip_w, clip_h) & cv::Rect(x, y, src_mat.cols, src_mat.rows) );
    if(draw_area.width == 0) {
      // Returns {0, 0, 0, 0} rectangle if there is no intersection
      // Nothing to draw...
      return;
    }

    cv::Mat dest_mat = mat(draw_area);

    draw_area.x = x < (int) clip_x ? clip_x - x : 0;
    draw_area.y = y < (int) clip_y ? clip_y - y : 0;
    src_mat = src_mat(draw_area);

    {
      boost::lock_guard<boost::mutex> image_lock(*rgba_buffer_mutex);
      float src_opacity = src.opacity / 255.f;
      bool is_src_opacity_full = (src.opacity == 255);
      for(int i = 0; i < draw_area.height; ++i)
      {
        color_channel* dest_row = dest_mat.ptr(i);
        color_channel* src_row = src_mat.ptr(i);
        for(int j = 0; j < draw_area.width; ++j)
        {
          color_channel* dpixel = dest_row + (j<<2); // (j*4) for RGBA pixels
          color_channel* spixel = src_row + (j<<2);
          // TODO performance concern: alpha_blend always?
          //      canvas opaque flag? (if loaded from jpg) src_mat.copyTo(dst_mat);
          if(is_src_opacity_full && spixel[3] == 0xff) {
            assign_color(dpixel, spixel);
          } else {
            alpha_blend(dpixel, spixel, src_opacity);
          }
        }
      }

      if(player)
        player->root_canvas_dirty = true;
    }
  }
  catch(cv::Exception& e)
  {
    // OpenCV already prints the error to stderr
  }
}

#if 0
void canvas::compose (int x, int y, canvas src)
{
  src.flush(); // flush to follow the standard

//   if(x < 0 || y < 0) {
//     return;
//   }

  // Destination vars
  long dl = std::max(x, (int)clip_x);
  long dr = std::min(clip_x+clip_w, width);
  long du = std::max(y, (int)clip_y);
  long dd = std::min(clip_y+clip_h, height);

//   std::cout
//   << " dl:" << dl
//   << " dr:" << dr
//   << " du:" << du
//   << " dd:" << dd
//   << std::endl;

  // Source vars
  long sl = std::max((long)src.crop_x - (x<0 ? x : 0), (long)(src.crop_x + clip_x) - x);
  long sr = std::min(src.crop_x + src.crop_w, src.width);
  long su = std::max((long)src.crop_y - (y<0 ? y : 0), (long)(src.crop_y + clip_y) - y);
  long sd = std::min(src.crop_y + src.crop_h, src.height);

//   std::cout
//   << " sl:" << sl
//   << " sr:" << sr
//   << " su:" << su
//   << " sd:" << sd
//   << std::endl;

  long rw = std::min(dr-dl, sr-sl);
  long rh = std::min(dd-du, sd-su);

  if(rw <= 0 || rh <= 0)
  {
    // std::cerr << "Nothing to draw..." << std::endl;
    return;
  }

  // Destination buffer
  color_channel* ddata = data();
  std::size_t dstride = stride;

  // Source buffer
  color_channel* sdata = src.data();
  std::size_t sstride = src.stride;

  // std::cerr << "rw:" << rw << " rh:" << rh << std::endl;

  {
    boost::lock_guard<boost::mutex> image_lock(*rgba_buffer_mutex);

    for(int i = 0; i < rh; ++i)
    {
      color_channel* drow = ddata + (i+du)*dstride;
      color_channel* srow = sdata + (i+su)*sstride;
      for(int j = 0; j < rw; ++j)
      {
        color_channel* dpixel = drow + (j+dl)*4;
        color_channel* spixel = srow + (j+sl)*4;
        // TODO performance concern: alpha_blend always?
        if(spixel[3] == 0xff) {
          assign_color(dpixel, spixel);
        } else {
          alpha_blend(dpixel, spixel);
        }
      }
    }

    if(player)
      player->root_canvas_dirty = true;
  }
}
#endif

void canvas::drawText(int x, int y, std::string text)
{
  // TODO
}

void canvas::measureText(int& dx, int& dy, std::string text)
{
  // TODO
}

// TODO Two dirty states?
void canvas::flush()
{
  if(player && player->root_canvas_dirty)
  {
    // Is this call too much obscure?
    async_redraw();
  }
}

} } } }

