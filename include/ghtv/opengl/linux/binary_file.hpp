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

#ifndef GHTV_OPENGL_LINUX_BINARY_FILE_HPP
#define GHTV_OPENGL_LINUX_BINARY_FILE_HPP

#include <vector>
#include <string>

namespace ghtv { namespace opengl { namespace linux_ {

/// Class for handling local and web files across the program.
/// All its member functions are synchronous, they will block and return only
/// when the operation is completed or a exception is thrown.
struct binary_file
{
  typedef std::vector<char> content; ///< Shortcut for vector of characters
  struct system_file_t {}; ///< Empty type for explicitly use the "system file" version of the constructor.

  binary_file(std::string const& ncl_root, std::string const& source_uri);
  binary_file(std::string const& ncl_root, std::string const& base, std::string const& source_uri);

  /// @warning Constructor for files used by the program itself, no checks for
  /// the NCL root will be performed. Use with caution.
  binary_file(system_file_t const&, std::string const& source_uri);

  ~binary_file();

  binary_file(binary_file const& other);
  binary_file& operator=(binary_file const& other);

  /// @throw std::runtime_error if resource could not be read.
  void load_content();
  /// Same as @ref load_content but add '\0' at the end.
  void load_content_as_c_str();
  void release_content();

  /// @note Invoke @ref load_content before.
  content const& file_content() const;
  /// @note Invoke @ref load_content before.
  char const* file_content_p() const;

  /// @note "system files" specified with file:// protocol are not considered local.
  /// Files specified as NCL media files are verified beforehand, so they may be
  /// considered local even if specified with the file:// protocol.
  bool is_local() const;

  std::string const& source_uri() const;

  std::string const& root() const;
  std::string url() const;
  /// @throw std::runtime_error if it is not a local file.
  std::string const& file_posix_path() const;

  /// This function should be invoked exactly once before using
  /// any @ref binary_file object.
  /// @throw std::runtime_error if called a second time.
  static void initialize_binary_files();

private:
  struct binary_file_impl;
  binary_file_impl* impl;
};

} } }

#endif
