#include "secondary.hpp"
#include "boost/asio.hpp"
#include "boost/asio/connect.hpp"
#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/dispatch.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/asio/read_until.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/write.hpp"
#include "boost/json.hpp"
#include "boost/json/parse_options.hpp"
#include "boost/log/trivial.hpp"
#include "boost/system/detail/errc.hpp"
#include "boost/system/is_error_code_enum.hpp"
#include "scconstants.h"
#include "sierrachart.h"
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

SCDLLName("Position Copy Plugin for Secondary Instance");

namespace boost
{
void tss_cleanup_implemented() {}
} // namespace boost

using tcp = boost::asio::ip::tcp;
using boost::asio::io_service;

struct SecondaryPlugin
{
  explicit SecondaryPlugin(std::string const &host, unsigned int port)
      : m_gotFirstUpdate(false), m_position(0), m_host(host), m_port(port),
        m_work(m_service), m_timer(m_service), m_socket(m_service),
        m_thread(std::bind(&SecondaryPlugin::threadFunc, this))
  {
    connect();
  }

  ~SecondaryPlugin()
  {
    BOOST_LOG_TRIVIAL(info)
        << "Stopping secondary client on port " << this->port();
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
  t_OrderQuantity32_64 position() const { return m_position; }
  bool gotFirstUpdate() const { return m_gotFirstUpdate; }

private:
  tcp::resolver::results_type resolve(std::string const &host,
                                      unsigned int port)
  {
    boost::system::error_code ec;
    std::ostringstream oss;
    oss << port;
    tcp::resolver resolver(m_service);
    return resolver.resolve(host, oss.str(), ec);
  }

  void connect()
  {
    auto endpoints = resolve(m_host, m_port);
    boost::asio::async_connect(m_socket, endpoints,
                               [this](const boost::system::error_code &ec,
                                      const tcp::endpoint &endpoint) {
                                 if (!ec)
                                 {
                                   BOOST_LOG_TRIVIAL(info)
                                       << "Connected to " << endpoint;
                                   readNext();
                                 }
                               });
  }

  void readNext()
  {
    boost::asio::async_read_until(
        m_socket, boost::asio::dynamic_buffer(m_buffer), '}',
        [this](const boost::system::error_code &ec, std::size_t bytesRead) {
          try
          {
            BOOST_LOG_TRIVIAL(trace) << "Received: " << m_buffer;
            boost::json::value jv = boost::json::parse(m_buffer);
            m_buffer.clear();
            if (auto p = jv.if_object())
            {
              if (auto position = p->if_contains("position"))
              {
                auto pos2 = position->to_number<double>();
                m_gotFirstUpdate = true;
                BOOST_LOG_TRIVIAL(info) << "Got position update" << pos2;
                m_position = pos2;
              }
            }
          }
          catch (std::exception const &e)
          {
            BOOST_LOG_TRIVIAL(error) << "Exception: " << e.what();
          }
          catch (...)
          {
            BOOST_LOG_TRIVIAL(error) << "Unknown exception";
          }
          if (ec)
          {
            BOOST_LOG_TRIVIAL(error) << "Closing socket due to error: " << ec;
            m_socket.close();
            auto timer =
                std::make_shared<boost::asio::deadline_timer>(m_service);
            timer->expires_from_now(boost::posix_time::seconds(5));
            timer->async_wait(
                [timer, this](const boost::system::error_code &ec) mutable {
                  connect();
                  timer.reset();
                });
          }
          else
          {
            readNext();
          }
        });
  }

  void threadFunc()
  {
    BOOST_LOG_TRIVIAL(info) << "Starting thread";

    while (!m_service.stopped())
    {
      try
      {
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

  bool m_gotFirstUpdate;
  t_OrderQuantity32_64 m_position;
  std::string m_host;
  unsigned int m_port;
  boost::asio::io_service m_service;
  boost::asio::io_service::work m_work;
  const tcp::resolver::results_type m_endpoints;
  tcp::socket m_socket;
  boost::asio::steady_timer m_timer;
  std::thread m_thread;
  std::string m_buffer;
};

const char *hello_secondary() { return "world"; }

SCSFExport scsf_SecondaryInstance(SCStudyInterfaceRef sc)
{
  SCInputRef Host = sc.Input[0];
  SCInputRef Port = sc.Input[1];

  if (sc.SetDefaults)
  {
    sc.GraphName = "Secondary instance for position copying";
    sc.StudyDescription = "Relay position updates to clients";
    sc.GraphRegion = 0;
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;

    Host.Name = "Host";
    Host.SetString("127.0.0.1");
    Host.SetDescription("IP/Host name");

    Port.Name = "Primary instance port";
    Port.SetInt(12050);
    Port.SetIntLimits(1024, 65536);
    Port.SetDescription("Port number");
  }
  else if (sc.LastCallToFunction)
  {
    auto ptr = (SecondaryPlugin *)sc.GetPersistentPointer(1);
    delete ptr;
    sc.SetPersistentPointer(1, nullptr);
  }
  else
  {
    auto ptr = (SecondaryPlugin *)sc.GetPersistentPointer(1);
    if (!ptr || ptr->port() != Port.GetInt())
    {
      ptr = new SecondaryPlugin(Host.GetString(), Port.GetInt());
      sc.SetPersistentPointer(1, ptr);
      sc.AddMessageToLog("Started client", 0);
    }
    if (ptr->gotFirstUpdate())
    {

      s_SCPositionData position;
      sc.GetTradePosition(position);

      if (!position.WorkingOrdersExist)
      {

        auto delta = ptr->position() - position.PositionQuantity;
        if (delta != 0)
        {
          sc.SendOrdersToTradeService = 1;
          sc.AllowMultipleEntriesInSameDirection = 1;
          sc.AllowEntryWithWorkingOrders = 0;
          sc.AllowOnlyOneTradePerBar = 0;

          s_SCNewOrder newOrder;
          newOrder.OrderQuantity = delta;
          newOrder.OrderType = SCT_ORDERTYPE_MARKET;
          newOrder.TimeInForce = SCT_TIF_DAY;
          BOOST_LOG_TRIVIAL(info)
              << "Current position " << position.PositionQuantity
              << " Adjusting by " << delta;
          if (delta > 0)
          {
            sc.BuyEntry(newOrder);
          }
          else if (delta < 0)
          {
            sc.SellEntry(newOrder);
          }
        }
      }
    }
  }
}
