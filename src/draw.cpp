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

#include <ghtv/opengl/linux/draw.hpp>
#include <ghtv/opengl/texture.hpp>
#include <ghtv/opengl/linux/global_state.hpp>

namespace ghtv { namespace opengl { namespace linux_  {

#ifndef GHTV_USE_GLUT
extern bool no_draw;
#endif
      
void draw()
{
#ifndef GHTV_USE_GLUT
  if(no_draw)
  {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    eglSwapBuffers(global_state.egl_display, global_state.egl_surface);
    return;
  }
#endif
#ifdef GHTV_USE_OPENMAX
  {
    boost::unique_lock<boost::mutex> l(global_state.pipeline_mutex);
    while(!global_state.busy_image_pipelines.empty())
    {
      global_state.pipeline_condition.wait(l);
    }
  }
#endif
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUniform1i(global_state.texture_location, 0);

  for(std::list<ghtv::opengl::active_player>::iterator
        first = global_state.main_state.active_players.begin()
        , last = global_state.main_state.active_players.end()
        ;first != last; ++first)
  {
    // std::cout << "texture " << std::endl;

    // TODO: DRAW MEDIA BORDER

    if(first->player && first->player->has_texture() && first->visible)
    {
      // std::cout << "texture " << first->player << std::endl;
      ghtv::opengl::texture* textures = 0;
      unsigned size = 0;
      first->player->get_textures(textures, size);

      for(unsigned i=0; i < size; ++i)
      {
        ghtv::opengl::texture& texture = textures[i];

        // TODO use all other properties (visible, tranparency, scroll, etc)
        GLfloat border_ratio[2] = {texture.border_size /(GLfloat)texture.width, texture.border_size /(GLfloat)texture.height};
        glUniform1f(global_state.z_location, (first->zindex)*std::numeric_limits<float>::epsilon());
        glUniform4f(global_state.border_color_location, texture.border_color.r, texture.border_color.g, texture.border_color.b, 1.0f);

        // TODO coordinates relative to texture information

        GLfloat left   = (GLfloat)(first->x + texture.x);
        GLfloat top    = (GLfloat)(first->y + texture.y);
        GLfloat right  = (GLfloat)(left + texture.width);
        GLfloat bottom = (GLfloat)(top + texture.height);

        GLfloat vertices[][2] = { {left , top}
                                , {right, top}
                                , {left , bottom}
                                , {right, bottom}};

        GLfloat texture_coords[][2] =  {  {0.0f - border_ratio[0], 0.0f - border_ratio[1]}
                                        , {1.0f + border_ratio[0], 0.0f - border_ratio[1]}
                                        , {0.0f - border_ratio[0], 1.0f + border_ratio[1]}
                                        , {1.0f + border_ratio[0], 1.0f + border_ratio[1]}};

        glUniform4f(global_state.clip_location
          , texture.clip.left / (GLfloat) texture.width , texture.clip.top / (GLfloat) texture.height
          , texture.clip.right / (GLfloat) texture.width, texture.clip.bottom / (GLfloat) texture.height);

        glActiveTexture(GL_TEXTURE0);
        texture.bind();

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texture_coords);
        glEnableVertexAttribArray(1);

        GLubyte indices[] = {0, 1, 2, 1, 2, 3};
        glDrawElements(GL_TRIANGLES, sizeof(indices)/sizeof(GLubyte)
                      , GL_UNSIGNED_BYTE, indices);

        assert(glGetError() == GL_NO_ERROR);
      }
    }
  }

#if defined(GHTV_USE_GLUT)
  glutSwapBuffers();
#else
  eglSwapBuffers(global_state.egl_display, global_state.egl_surface);
#endif
}

} } }
