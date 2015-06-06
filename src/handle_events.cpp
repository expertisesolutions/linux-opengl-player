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

#include <ghtv/opengl/linux/handle_events.hpp>

#include <ghtv/opengl/linux/global_state.hpp>

#include <gntl/algorithm/structure/port/start_action_traits.ipp>
#include <gntl/algorithm/structure/port/start.ipp>
#include <gntl/algorithm/structure/component/start.ipp>
#include <gntl/algorithm/structure/component/stop.ipp>
#include <gntl/algorithm/structure/component/pause.ipp>
#include <gntl/algorithm/structure/component/resume.ipp>
#include <gntl/algorithm/structure/component/abort.ipp>
#include <gntl/algorithm/structure/media/start_normal_action.hpp>
#include <gntl/algorithm/structure/media/start.ipp>
#include <gntl/algorithm/structure/media/start_action_traits.ipp>
#include <gntl/algorithm/structure/media/stop.ipp>
#include <gntl/algorithm/structure/media/stop_action_traits.ipp>
#include <gntl/algorithm/structure/media/resume.ipp>
#include <gntl/algorithm/structure/media/resume_action_traits.ipp>
#include <gntl/algorithm/structure/media/pause.ipp>
#include <gntl/algorithm/structure/media/pause_action_traits.ipp>
#include <gntl/algorithm/structure/media/abort.ipp>
#include <gntl/algorithm/structure/media/abort_action_traits.ipp>
#include <gntl/algorithm/structure/media/set.ipp>
#include <gntl/algorithm/structure/media/set_action_traits.ipp>
#include <gntl/algorithm/structure/media/select.ipp>
#include <gntl/algorithm/structure/switch/start_normal_action.hpp>
#include <gntl/algorithm/structure/switch/start.ipp>
#include <gntl/algorithm/structure/switch/start_action_traits.ipp>
#include <gntl/algorithm/structure/switch/stop.ipp>
#include <gntl/algorithm/structure/switch/stop_action_traits.ipp>
#include <gntl/algorithm/structure/switch/abort.ipp>
#include <gntl/algorithm/structure/switch/abort_action_traits.ipp>
#include <gntl/algorithm/structure/context/evaluate_links.ipp>

namespace ghtv { namespace opengl { namespace linux_ {

void handle_events()
{
  gntl::algorithm::structure::media::dimensions const screen_dimensions
    = {0, 0, global_state.width, global_state.height, 0};

  typedef gntl::concept::structure::document_traits
    <state::main_state_type::document_type> document_traits;
  while(document_traits::pending_events(*global_state.main_state.document))
  {
    document_traits::event_type event = document_traits::top_event
      (*global_state.main_state.document);
    document_traits::pop_event (*global_state.main_state.document);

    try
    {
      gntl::algorithm::structure::context::evaluate_links
        (gntl::ref(global_state.main_state.document->body)
         , global_state.main_state.document->body_location()
         , gntl::ref(*global_state.main_state.document)
         , event, screen_dimensions);

      if(event.event_ == gntl::event_enum::attribution
         && event.transition == gntl::transition_enum::starts
         && event.interface_)
      {
        std::cout << "attribution event" << std::endl;
        typedef document_traits::media_lookupable media_lookupable;
        media_lookupable lookupable = document_traits::media_lookup(*global_state.main_state.document);
        typedef gntl::concept::lookupable_traits<media_lookupable> lookupable_traits;
        typedef lookupable_traits::result_type lookup_result;

        lookup_result r = lookupable_traits::lookup(lookupable, event.component_identifier);
        typedef lookupable_traits::value_type media_type;

        if(r != lookupable_traits::not_found(lookupable))
        {
          typedef document_traits::context_lookupable context_lookupable;
          context_lookupable lookupable = document_traits::context_lookup(*global_state.main_state.document);
          typedef gntl::concept::lookupable_traits<context_lookupable> lookupable_traits;
          typedef lookupable_traits::result_type lookup_result;

          gntl::algorithm::structure::media::commit_set
            (*r, *event.interface_
             , gntl::ref(*global_state.main_state.document));
        }
      }
    }
    catch(...)
    {
      std::cerr << "Exception while handling events" << std::endl;
    }
  }
}

} } }
