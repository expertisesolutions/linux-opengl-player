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

#include <ghtv/opengl/linux/url_join.hpp>

#include <Uri.h>
#include <stdexcept>

namespace ghtv { namespace opengl { namespace linux_ {

std::string url_join(std::string const& base, std::string const& relative)
{
  std::string output;
  bool ok = false;

  UriParserStateA state;
  UriUriA uriBase;
  state.uri = &uriBase;
  if(uriParseUriA(&state, base.c_str()) == URI_SUCCESS)
  {
    UriUriA uriRelative;
    state.uri = &uriRelative;
    if(uriParseUriA(&state, relative.c_str()) == URI_SUCCESS)
    {
      UriUriA result;
      if(uriAddBaseUriA(&result, &uriRelative, &uriBase) == URI_SUCCESS)
      {
        int charsRequired = 0;
        if(uriToStringCharsRequiredA(&result, &charsRequired) == URI_SUCCESS)
        {
          charsRequired++;
          char* buf = new char[charsRequired];
          if(uriToStringA(buf, &result, charsRequired, NULL) == URI_SUCCESS)
          {
            // Everything went OK
            output = buf;
            ok = true;
            // TODO: Normalize URL (Optional flag)
          }
          delete[] buf;
        }
        uriFreeUriMembersA(&result);
      }
      uriFreeUriMembersA(&uriRelative);
    }
    uriFreeUriMembersA(&uriBase);
  }

  if(!ok)
  {
    throw std::runtime_error(std::string(__func__) + ": could not join base \"" + base + "\" with relative \"" + relative + "\"");
  }

  return output;
}

} } }

