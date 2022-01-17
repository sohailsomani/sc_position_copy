#include "primary.hpp"
#include "boost/asio.hpp"
#include "boost/asio/dispatch.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/asio/write.hpp"
#include "boost/log/trivial.hpp"
#include "boost/system/detail/errc.hpp"
#include "boost/system/is_error_code_enum.hpp"
#include "sierrachart.h"
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

SCDLLName("Position Copy Plugin for Primary Instance");

namespace boost
{
void tss_cleanup_implemented() {}
} // namespace boost

using tcp = boost::asio::ip::tcp;
using boost::asio::io_service;

struct Connection
{
  explicit Connection(io_service &service, tcp::socket socket)
      : m_service(service), m_timer(m_service), m_socket(std::move(socket))
  {
    boost::system::error_code ec;
    auto endpoint = m_socket.remote_endpoint(ec);
    if (!ec)
    {
      BOOST_LOG_TRIVIAL(info) << "New connection from " << endpoint;
    }
    else
    {
      BOOST_LOG_TRIVIAL(error)
          << "Unable to get remote endpoint from socket! " << ec.message();
    }
    sendPing();
  }

  void sendTick(float tick)
  {
    std::ostringstream ss;
    ss << "TICK-" << tick;
    std::string msg(ss.str());
    boost::asio::async_write(
        m_socket, boost::asio::buffer(msg),
        [this](boost::system::error_code const &ec,
               std::size_t bytesTransferred) { processErrorCode(ec); });
  }

private:
  void sendPing()
  {
    m_timer.expires_after(boost::asio::chrono::seconds(1));
    m_timer.async_wait([this](const boost::system::error_code &ec) {
      boost::asio::async_write(m_socket, boost::asio::buffer("PING"),
                               [this](const boost::system::error_code &ec,
                                      size_t bytes_transferred) {
                                 if (processErrorCode(ec))
                                   sendPing();
                               });
    });
  }

  bool processErrorCode(const boost::system::error_code &ec)
  {
    bool shouldKeepGoing = ec != boost::asio::error::eof &&
                           ec != boost::asio::error::connection_reset;
    if (!shouldKeepGoing)
    {
      boost::system::error_code ec;
      auto endpoint = m_socket.remote_endpoint(ec);
      BOOST_LOG_TRIVIAL(info) << "Connection lost: " << endpoint;
    }
    return shouldKeepGoing;
  }

  io_service &m_service;
  boost::asio::steady_timer m_timer;
  tcp::socket m_socket;
};

struct PrimaryPlugin
{
  explicit PrimaryPlugin(unsigned int port)
      : m_port(port), m_work(m_service), m_endpoint(tcp::v4(), port),
        m_acceptor(m_service, m_endpoint),
        m_thread([this]() { this->threadFunc(); })
  {
    BOOST_LOG_TRIVIAL(info)
        << "Creating new primary server on port " << this->port();
  }

  ~PrimaryPlugin()
  {
    BOOST_LOG_TRIVIAL(info)
        << "Stopping primary server on port " << this->port();
    m_service.stop();

    try
    {
      BOOST_LOG_TRIVIAL(info) << "Joining thread";
      if (m_thread.joinable())
        m_thread.join();
    }
    catch (...)
    {
      BOOST_LOG_TRIVIAL(error) << "Exception when joining thread";
    }
  }

  unsigned int port() const { return m_port; }
  void sendTick(float last)
  {
    for (auto &conn : m_connections)
    {
      conn->sendTick(last);
    }
  }

private:
  void accept()
  {
    m_acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec)
          {
            m_connections.push_back(
                std::make_unique<Connection>(m_service, std::move(socket)));
          }
          accept();
        });
  }

  void threadFunc()
  {
    BOOST_LOG_TRIVIAL(info) << "Starting thread";

    while (!m_service.stopped())
    {
      try
      {
        accept();
        m_service.run();
      }
      catch (std::exception const &e)
      {
        BOOST_LOG_TRIVIAL(error)
            << "Exception in io_service::run: " << e.what();
      }
      catch (...)
      {
        BOOST_LOG_TRIVIAL(error) << "Unknown exception in io_service::run";
      }
    }
    BOOST_LOG_TRIVIAL(info) << "Thread done";
  }

  unsigned int m_port;
  boost::asio::io_service m_service;
  boost::asio::io_service::work m_work;
  tcp::endpoint m_endpoint;
  tcp::acceptor m_acceptor;
  std::vector<std::unique_ptr<Connection>> m_connections;
  std::thread m_thread;
};

const char *hello_primary() { return "world"; }

static std::unordered_map<s_sc *, std::unique_ptr<PrimaryPlugin>> s_instances;

SCSFExport scsf_PrimaryInstance(SCStudyInterfaceRef sc)
{
  SCInputRef Port = sc.Input[0];

  if (sc.SetDefaults)
  {
    sc.GraphName = "Primary instance for position copying";
    sc.StudyDescription = "Relay position updates to clients";
    sc.GraphRegion = 0;
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;

    Port.Name = "Server port";
    Port.SetInt(12050);
    Port.SetIntLimits(1024, 65536);
    Port.SetDescription("Port number");
  }
  else if (sc.LastCallToFunction)
  {
    sc.AddMessageToLog("Last call", 0);
    auto it = s_instances.find(&sc);
    if (it != s_instances.end())
    {
      auto ptr = std::move(it->second);
      sc.AddMessageToLog("Stopping server", 0);
      s_instances.erase(it);
      ptr.reset();
    }
  }
  else
  {
    auto it = s_instances.find(&sc);
    if (it == s_instances.end() || (it->second->port() != Port.GetInt()))
    {
      it = s_instances
               .insert(std::make_pair(
                   &sc, std::make_unique<PrimaryPlugin>(Port.GetInt())))
               .first;
      sc.AddMessageToLog("Started server", 0);
    }
    it->second->sendTick(sc.Close[0]);
  }
}
