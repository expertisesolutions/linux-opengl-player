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

#include <ghtv/opengl/linux/idle_update.hpp>

#include <ghtv/opengl/linux/draw.hpp>
#include <ghtv/opengl/linux/handle_events.hpp>

#include <boost/atomic.hpp>

#include <iostream>

namespace ghtv { namespace opengl { namespace linux_ {

namespace {
boost::atomic_bool redraw(true);
boost::atomic_bool has_new_events(true);
}

// TODO glutPostRedisplay instead ???

void glut_idle_update_function()
{
  if(has_new_events) {
    has_new_events = false;
    handle_events();
  }
  if(redraw) {
    redraw = false;
    draw();
  }
}

void async_redraw()
{
  redraw = true;
}

void async_signal_new_event()
{
  has_new_events = true;
}

} } }
