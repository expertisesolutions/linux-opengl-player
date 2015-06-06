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

#include <ghtv/opengl/linux/sound_player.hpp>

#include <glib.h>
#include <gst/gst.h>
#include <iostream>
#include <cstdlib>

namespace ghtv { namespace opengl { namespace linux_ {

namespace {
bool gstreamer_initialized = false;

void initialize_gstreamer() {
  int argc = 0;
  char** argv = 0;
  gst_init(&argc, &argv);
  gstreamer_initialized = true;
}
} // end of anonymous namespace

struct sound_player::sound_player_data
{
  GstElement* pipeline;

  sound_player_data()
    : pipeline(0)
  {  }
};

sound_player::sound_player(binary_file const& file)
  : m(new sound_player_data())
{
  if(!gstreamer_initialized) {
    initialize_gstreamer();
  }

  std::string parse_str = "playbin uri=" + file.url();
  std::cout << "sound_player: " << parse_str << std::endl;
  m->pipeline = gst_parse_launch(parse_str.c_str(), NULL);
  if(!m->pipeline) {
    std::cerr << "Could not create pipeline to " << file.source_uri() << std::endl;
    return;
  }
}

sound_player::~sound_player()
{
  if(m->pipeline) {
    gst_element_set_state(m->pipeline, GST_STATE_NULL);
    gst_object_unref(m->pipeline);
  }
  delete m;
}

void sound_player::start()
{
  if(!m->pipeline)
    return;

  if(gst_element_set_state(m->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
  {
    std::cerr << "Could not start playing" << std::endl;
  }
}

void sound_player::pause()
{
  if(!m->pipeline)
    return;

  if(gst_element_set_state(m->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
  {
    std::cerr << "Could not pause sound"  << std::endl;
  }
}

void sound_player::resume()
{
  if(!m->pipeline)
    return;

  if(gst_element_set_state(m->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
  {
    std::cerr << "Could not resume sound" << std::endl;
  }
}

bool sound_player::set_property(std::string const& name, std::string const& value)
{
  if(!m->pipeline)
    return false;

  if(name == "soundLevel") {
    if(value.empty())
      return false;
    // Not checking the string. If an invalid value is supplied most probaly
    // the audio will be muted (atof will return 0).
    gdouble v = std::atof(value.c_str());
    if(*value.rbegin() == '%') {
      v /= 100;
    }
    if(v >= 0.0 && v <= 1.0) {
      g_object_set(m->pipeline, "volume", v, NULL);
      return true;
    }
  }

  // TODO: balanceLevel, trebleLevel, bassLevel

  return false;
}

} } }
