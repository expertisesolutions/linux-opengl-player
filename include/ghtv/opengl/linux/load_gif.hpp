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

#ifndef LOAD_GIF_HPP
#define LOAD_GIF_HPP

#ifdef GHTV_USE_GLUT
#include <GL/glut.h>
#endif

#ifdef GHTV_USE_OPENGLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include <ghtv/opengl/texture.hpp>
#include <ghtv/opengl/linux/binary_file.hpp>
#include <vector>

namespace ghtv { namespace opengl { namespace linux_ {

void load_gif_to_rgba(binary_file const& file, std::vector<unsigned char>& data, std::size_t& width, std::size_t& height);

texture load_gif(binary_file const& file);

} } }

#endif
