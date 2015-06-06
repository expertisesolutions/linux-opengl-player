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

#include <ghtv/opengl/linux/lua/socket.hpp>

#include <ghtv/opengl/linux/lua_player.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio.hpp>
#include <sstream>
#include <deque>

using boost::asio::ip::tcp;

namespace ghtv { namespace opengl { namespace linux_ { namespace lua {

struct socket::socket_impl
{
  lua_player* player;
  int connection_id;
  std::string host;
  int port;

  boost::thread socket_thread;
  boost::asio::io_service io_service;
  tcp::resolver resolver;
  tcp::socket socket_hdl;

  static const std::size_t buffer_size = 512;
  char incoming_data_buffer[buffer_size];

  typedef std::deque<std::string> message_deque;
  message_deque outgoing_messages;


  socket_impl(lua_player* player_, std::string const& host_, int port_, int id_)
    : player(player_)
    , connection_id(id_)
    , host(host_)
    , port(port_)
    , io_service()
    , resolver(io_service)
    , socket_hdl(io_service)
  {  }

  std::string format_error_msg(std::string const& desc, boost::system::error_code const& err)
  {
    std::stringstream ss;
    ss << "Error on connection (" << connection_id << ") [" << host << ":" << port << "]: ";
    ss << desc << " (" << err.message() << ")";
    return ss.str();
  }

  /// Function for starting and running the asynchronous socket.
  void run_socket()
  {
    try
    {
      std::stringstream ss;
      ss << port;
      tcp::resolver::query query(host, ss.str());

      // Request an asynchronous resolve to translate the server and service names
      // into a list of endpoints.
      resolver.async_resolve(query,
          boost::bind(&socket_impl::handle_resolve, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));

      // The io_service.run() loop. Blocks the thread until there are no more
      // pending requests.
      // TODO: Is it OK to call thread::interrupt() before io_service.run() returning?
      io_service.run();

      // Commit suicide.
      boost::lock_guard<boost::mutex> lock(player->sockets_mutex);
      boost::this_thread::interruption_point();
      socket_thread.detach();
      player->delete_socket(connection_id);
    }
    catch(boost::thread_interrupted& e)
    {
      std::cout << "lua::socket (" << connection_id << ") interrupted" << std::endl;
    }
  }

  /// Sends the given message through the socket connection.
  /// The socket must be connect.
  bool write(std::string const& value, std::string& error_msg)
  {
    // Posting the event to ensure that it will be executed in the
    // io_service.run() thread, thus avoiding thread concurrency problems.
    // The string is binded by value (copy), so it will be available when the
    // do_write function is called.
    io_service.post(boost::bind(&socket_impl::do_write, this, value));
    return true;
  }

  // ===========================================================================
  // All these methods run in the io_service.run() thread (so no synchronization
  // is required):
  //

  /// Signals the completion of a resolve request. If there are no errors it
  /// request a connection to the given endpoint.
  void handle_resolve(boost::system::error_code const& err, tcp::resolver::iterator endpoint_iterator)
  {
    if(!err)
    {
      // After resolving the host location we request a connection.
      boost::asio::async_connect(socket_hdl, endpoint_iterator,
          boost::bind(&socket_impl::handle_connect, this,
            boost::asio::placeholders::error));
    }
    else
    {
      std::string error = format_error_msg("Error while resolving host address", err);
      // Inform error to the lua script
      player->add_lua_input_event_to_execution_queue(
        lua_player::lua_input_event::tcp_event_connect(host, port, -1, error)
      );
      std::cerr << error << std::endl;
    }
  }

  /// Signals the completion of a connection request. If there are no errors
  /// starts reading incoming data. Also informs the lua script that the
  /// connection has been established.
  void handle_connect(boost::system::error_code const& err)
  {
    if(!!err)
    {
      std::string error = format_error_msg("Error while connecting to host", err);
      // Inform error to the lua script
      player->add_lua_input_event_to_execution_queue(
        lua_player::lua_input_event::tcp_event_connect(host, port, -1, error)
      );
      std::cerr << error << std::endl;
      return;
    }

    // Inform successful connection to the lua script.
    player->add_lua_input_event_to_execution_queue(
      lua_player::lua_input_event::tcp_event_connect(host, port, connection_id)
    );

    // Request reading of incoming data.
    async_read_data();
  }

  /// Signals the completion of a reading request. If there are no errors
  /// informs the lua script of the received content. Also continue requisting
  /// the reading of incoming data.
  void handle_read(boost::system::error_code const& err, std::size_t bytes_transferred)
  {
    if(!!err)
    {
      std::string error = format_error_msg("Error while reading incoming data", err);
      // Inform error to the lua script
      player->add_lua_input_event_to_execution_queue(
        lua_player::lua_input_event::tcp_event_data(connection_id, "", error)
      );
      std::cerr << error << std::endl;
      return;
    }

    std::string msg_string(incoming_data_buffer, bytes_transferred);

    std::cout << "msg_string:" << msg_string << std::endl;

    // Passing data content to the lua script.
    player->add_lua_input_event_to_execution_queue(
      lua_player::lua_input_event::tcp_event_data(connection_id, msg_string)
    );

    // Continue requisting the reading of incoming data until io_service is
    // stopped or an error occours.
    async_read_data();
  }

  // Request the reading of available data received by the socket.
  void async_read_data()
  {
    socket_hdl.async_read_some(
      boost::asio::buffer(incoming_data_buffer, buffer_size),
      boost::bind(
        &socket_impl::handle_read, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred
      )
    );
  }

  /// Request the writting of the message in front of the message queue.
  void async_write_data()
  {
    boost::asio::async_write(
      socket_hdl,
      boost::asio::buffer(outgoing_messages.front().data(), outgoing_messages.front().length()),
      boost::bind(&socket_impl::handle_write, this, boost::asio::placeholders::error)
    );
  }

  /// Signals the completion of a write. If there are no errors and messages
  /// still remain in the message queue, it takes care of requesting the
  /// writting of the next message.
  void handle_write(const boost::system::error_code& err)
  {
    if(!!err)
    {
      std::string error = format_error_msg("Error while sending data", err);
      // Inform error to the lua script
      player->add_lua_input_event_to_execution_queue(
        lua_player::lua_input_event::tcp_event_data(connection_id, "", error)
      );
      std::cerr << error << std::endl;
      return;
    }

    // Successfully writted the front of the message queue,
    // so we are popping it out.
    outgoing_messages.pop_front();

    // If there are other messages in the message queue, we need to request the
    // writting of the next message. This cicle goes on until there are no more
    // messages in the message queue (or until an error occours).
    // This worflow is important because a do_write may occour before a write
    // request is finished, and it is not a good ideia to have two write
    // requests at the same time, in order to ensure the correct order.
    if(!outgoing_messages.empty())
    {
      async_write_data();
    }
  }

  /// Proper implementation of the "write" function to be called by the
  /// io_service.run() thread. Adds given message to the message queue and, if
  /// there is not a write request already in place, request that the message be
  /// writting to the
  /// socket.
  void do_write(std::string msg)
  {
    bool no_requested_writes = outgoing_messages.empty();
    outgoing_messages.push_back(msg);

    // If the message queue was not empty then the writing of the front message
    // has already been requested, so we are not requesting it again.
    // The handle_write function will take care of requesting the writing of the
    // other messages when the front message is finally consumed.
    if(no_requested_writes)
    {
      // Since there is no requested write, we can safely request the writting
      // of the newly inserted message.
      async_write_data();
    }
  }
};

socket::socket(lua_player* player, std::string const& host, int port, int id)
  : pimpl(new socket_impl(player, host, port, id))
{ }

socket::~socket()
{
  delete pimpl;
}

void socket::start()
{
  pimpl->socket_thread = boost::thread(&socket_impl::run_socket, pimpl);
}

void socket::stop()
{
  pimpl->io_service.stop();
}

void socket::join()
{
  pimpl->socket_thread.join();
}

void socket::interrupt()
{
  pimpl->socket_thread.interrupt();
}

bool socket::write(std::string const& value, std::string& error_msg)
{
  return pimpl->write(value, error_msg);
}

} } } }
