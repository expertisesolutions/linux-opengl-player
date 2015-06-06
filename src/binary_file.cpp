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

#include <ghtv/opengl/linux/binary_file.hpp>

#include <ghtv/opengl/linux/url_join.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

// #define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/easy.h>

#include <stdexcept>
#include <sstream>
#include <fstream>
#include <cassert>

namespace ghtv { namespace opengl { namespace linux_ {

namespace {

bool libcurl_initialized = false;

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  // TODO: Use stream object instead of vector??
  binary_file::content* vec = (binary_file::content*) userp;
  char* c = (char*) contents;
  vec->insert(vec->end(), c, c + (size*nmemb));
  return size * nmemb;
}

} // end of anonymous namespace

struct binary_file::binary_file_impl
{
  enum source_type {
    UNKNOWN,
    POSIX_PATH, // Default to this when other patterns are not recognized
    FILE_URL,
    GENERIC_URL,
  };

  static source_type get_source_type(std::string const& str)
  {
    if(boost::algorithm::starts_with(str, "http://")
      || boost::algorithm::starts_with(str, "https://")
      || boost::algorithm::starts_with(str, "ftp://")
      || boost::algorithm::starts_with(str, "ftps://")
    ) {
      // TODO: relative URI
      return GENERIC_URL;
    }
//     else if(boost::algorithm::starts_with(str, "rstp://"))
//     {
//       // TODO
//       throw std::runtime_error("rstp URI not implemented");
//     }
//     else if(boost::algorithm::starts_with(str, "rtp://"))
//     {
//       // TODO
//       throw std::runtime_error("rtp URI not implemented");
//     }
//     else if(boost::algorithm::starts_with(str, "ncl-mirror://"))
//     {
//       // TODO
//       throw std::runtime_error("ncl-mirror URI not implemented");
//     }
//     else if(boost::algorithm::starts_with(str, "sbtvd-ts://"))
//     {
//       // TODO
//       throw std::runtime_error("sbtvd-ts URI not implemented");
//     }
    else if(boost::algorithm::starts_with(str, "file://"))
    {
      return FILE_URL;
    }
    else
    {
      return POSIX_PATH;
    }
  }

  // If the file comes from a NCL media we need to do some verifications
  binary_file_impl(std::string const& ncl_root, std::string const& source_uri, std::string const& base = "")
    : type(UNKNOWN)
    , file_content()
    , source_uri(source_uri)
    , root()
    , uri(source_uri)
  {
    root = boost::filesystem::canonical(ncl_root).string();
    if(*root.rbegin() != '/')
      root += '/';

    type = get_source_type(uri);

    // Applying the base path only if the source is relative and a base path is
    // supplied.
    // The source is relative only if it have defaulted to POSIX_PATH type and
    // does not start with with a slash (/)
    if(!base.empty() && type == POSIX_PATH && uri[0] != '/')
    {
      if(base[0] != '/') // Small shortcut for the most commom case (avoid get_source_type comparisons)
      {
        // Base may not be a POSIX path. Rechecking type...
        type = get_source_type(base);
      }

      // Join URLs
      if(type == POSIX_PATH)
      {
        // Simpler and faster join for POSIX paths
        uri.insert(0, *base.rbegin() == '/' ? base : base + '/');
      }
      else
      {
        // Complete URI parse and join following RFC 3986
        uri = url_join(base, uri);
      }
    }

    if(type == POSIX_PATH || type == FILE_URL)
    {
      if(type == FILE_URL)
      {
        // File URL is converted to POSIX path
        type = POSIX_PATH;
        uri = uri.substr(7, std::string::npos);
      }

      {
        boost::filesystem::path uri_path(uri);
        boost::filesystem::path absolute_file_path;
        if(base.empty() || uri_path.is_relative())
        {
          // Even if the uri_path is absolute, if a base is not supplied its
          // resolution must be rooted to the NCL root path.
          absolute_file_path = root;
          absolute_file_path /= uri_path.relative_path();
        }
        else
        {
          // If an absolute base is supplied we do not make the file resolution
          // rooted to the NCL directory. It is the most common case and we use
          // it to make paths relative to media subcomponents without having to
          // remove the beginning of the absolute paths used to handle them.
          // Otherwise paths would be like this (/ncl/root/ncl/root/subdir/file).
          //
          // By the way, the file still needs to be inside the NCL root directory
          // in order to avoid throwing a exception.
          absolute_file_path = uri_path;
        }
        uri = boost::filesystem::canonical(absolute_file_path).string();
      }

      // Check if the file is inside the NCL root path
      if(!boost::algorithm::starts_with(uri, root))
      {
        throw std::runtime_error(uri + " is outside the NCL root path");
      }
    }
  }

  binary_file_impl(system_file_t const&, std::string const& source_uri)
    : type(UNKNOWN)
    , file_content()
    , source_uri(source_uri)
    , root()
    , uri(source_uri)
  {
    type = get_source_type(uri);
  }

  binary_file_impl(binary_file_impl const& other)
    : type(other.type)
    , file_content(other.file_content)
    , source_uri(other.source_uri)
    , root(other.root)
    , uri(other.uri)
  {  }

  binary_file_impl& operator=(binary_file_impl const& other)
  {
    type = other.type;
    file_content = other.file_content;
    source_uri = other.source_uri;
    root = other.root;
    uri = other.uri;
    return *this;
  }

  void load_content(bool add_leading_null)
  {
    switch(type)
    {
    case POSIX_PATH: load_local_file_data(); break;
    case FILE_URL:
    case GENERIC_URL: load_url_data(); break;
    default:
      throw std::runtime_error("Unknown file source: " + uri);
    }

    if(add_leading_null) {
      file_content.push_back('\0');
    }
  }

  void release_content()
  {
    // Force memory release
    content().swap(file_content);
  }

  std::string url() const
  {
    if(type == POSIX_PATH) {
      return "file://" + uri;
    }
    return uri;
  }

  std::string const& file_posix_path() const
  {
    if(type != POSIX_PATH) {
      throw std::runtime_error(source_uri + " is not a local file");
    }
    return uri;
  }

private:
  void load_url_data()
  {
    CURL* curl = curl_easy_init();
    if(!curl) {
      throw std::runtime_error("Could not create curl resolver for " + uri);
    }

    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file_content);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl); // Free memory
    if(res != CURLE_OK)
    {
      std::stringstream ss;
      ss << "Could not get \"" << uri << "\". Error " << res << ": " << curl_easy_strerror(res);
      throw std::runtime_error(ss.str());
    }
  }

  void load_local_file_data()
  {
    std::ifstream file(uri.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if(!file.is_open())
    {
      throw std::runtime_error("Could not open: " + uri);
    }
    long fsize = file.tellg();
    file.seekg(0);
    file_content.resize(fsize);
    file.read(&file_content[0], fsize);
    if(!file.good())
    {
      release_content();
      throw std::runtime_error("Could not read: " + uri);
    }
  }

public:
  source_type type;
  content file_content;
  std::string source_uri;
  std::string root;
  std::string uri;
};

binary_file::binary_file(std::string const& ncl_root, std::string const& source_uri)
{
  assert(!!libcurl_initialized);
  impl = new binary_file_impl(ncl_root, source_uri);
}

binary_file::binary_file(std::string const& ncl_root, std::string const& base, std::string const& source_uri)
{
  assert(!!libcurl_initialized);
  impl = new binary_file_impl(ncl_root, source_uri, base);
}

binary_file::binary_file(system_file_t const&, std::string const& source_uri)
{
  assert(!!libcurl_initialized);
  impl = new binary_file_impl(system_file_t(), source_uri);
}

binary_file::~binary_file()
{
  delete impl;
}

binary_file::binary_file(binary_file const& other)
  : impl(new binary_file_impl(*other.impl))
{ }

binary_file& binary_file::operator=(binary_file const& other)
{
  *impl = *other.impl;
  return *this;
}

void binary_file::load_content()
{
  impl->load_content(false);
}

void binary_file::load_content_as_c_str()
{
  impl->load_content(true);
}

void binary_file::release_content()
{
  impl->release_content();
}

binary_file::content const& binary_file::file_content() const
{
  return impl->file_content;
}

const char* binary_file::file_content_p() const
{
  return &(impl->file_content.front());
}

bool binary_file::is_local() const
{
  return impl->type == binary_file_impl::POSIX_PATH;
}

std::string const& binary_file::source_uri() const
{
  return impl->source_uri;
}

std::string const& binary_file::root() const
{
  return impl->root;
}

std::string binary_file::url() const
{
  return impl->url();
}

std::string const& binary_file::file_posix_path() const
{
  return impl->file_posix_path();
}

void binary_file::initialize_binary_files()
{
  if(libcurl_initialized) {
    throw std::runtime_error("binary_files already initialized.");
  }
  if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    throw std::runtime_error("Could not initialzie libcurl.");
  }
  libcurl_initialized = true;
}

} } }
