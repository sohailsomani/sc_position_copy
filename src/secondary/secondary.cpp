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
#include "boost/chrono/duration.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/date_time/posix_time/time_formatters.hpp"
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
      : m_host(host), m_port(port), m_work(m_service),
        m_reconnectTimer(m_service), m_socket(m_service),
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
  t_OrderQuantity32_64 position()
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_position;
  }

  bool gotFirstUpdate()
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_gotFirstUpdate;
  }

  std::string serverChartbook()
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_serverChartbook;
  }

  boost::posix_time::ptime timeOfLastMessage()
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_lastMessageTime;
  }

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
    m_reconnectTimer.expires_from_now(boost::asio::chrono::seconds(5));
    m_reconnectTimer.async_wait(
        [this](const boost::system::error_code &ec) mutable {
          auto diff = boost::posix_time::microsec_clock::local_time() -
                      m_lastMessageTime;
          if (diff.total_seconds() > 10)
          {
            m_socket.close();
            auto endpoints = resolve(m_host, m_port);
            BOOST_LOG_TRIVIAL(info)
                << "Connecting to " << m_host << ":" << m_port;
            boost::asio::async_connect(
                m_socket, endpoints,
                [this](const boost::system::error_code &ec,
                       const tcp::endpoint &endpoint) {
                  if (!ec)
                  {
                    BOOST_LOG_TRIVIAL(info) << "Connected to " << endpoint;
                    readNext();
                  }
                  else
                  {
                    BOOST_LOG_TRIVIAL(info) << "Connection failure " << ec;
                    m_socket.close();
                  }
                });
          }
          connect();
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
              std::lock_guard<std::mutex> lock(m_mutex);

              m_lastMessageTime =
                  boost::posix_time::microsec_clock::local_time();
              if (auto cb = p->if_contains("cb"))
              {
                m_serverChartbook = cb->as_string();
              }
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

  std::mutex m_mutex;
  bool m_gotFirstUpdate = false;
  boost::posix_time::ptime m_lastMessageTime =
      boost::posix_time::microsec_clock::local_time();
  std::string m_serverChartbook;
  t_OrderQuantity32_64 m_position = 0;
  std::string m_host;
  unsigned int m_port;
  boost::asio::io_service m_service;
  boost::asio::io_service::work m_work;
  const tcp::resolver::results_type m_endpoints;
  tcp::socket m_socket;
  boost::asio::steady_timer m_reconnectTimer;
  std::thread m_thread;
  std::string m_buffer;
};

const char *hello_secondary() { return "world"; }

SCSFExport scsf_SecondaryInstance(SCStudyInterfaceRef sc)
{
  SCSubgraphRef Subgraph_ConnectionInfo = sc.Subgraph[0];

  SCInputRef Host = sc.Input[0];
  SCInputRef Port = sc.Input[1];

  SCInputRef Input_HorizontalPosition = sc.Input[2];
  SCInputRef Input_VerticalPosition = sc.Input[3];
  SCInputRef Input_DrawAboveMainPriceGraph = sc.Input[4];
  SCInputRef Input_UseBoldFont = sc.Input[5];
  SCInputRef Input_TransparentLabelBackground = sc.Input[6];
  SCInputRef Input_TextSize = sc.Input[7];

  try
  {

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

      Subgraph_ConnectionInfo.Name = "Server info";
      Subgraph_ConnectionInfo.LineWidth = 20;
      Subgraph_ConnectionInfo.DrawStyle = DRAWSTYLE_CUSTOM_TEXT;
      Subgraph_ConnectionInfo.PrimaryColor = RGB(0, 0, 0);       // black
      Subgraph_ConnectionInfo.SecondaryColor = RGB(255, 127, 0); // Orange
      Subgraph_ConnectionInfo.SecondaryColorUsed = true;
      Subgraph_ConnectionInfo.DisplayNameValueInWindowsFlags = 1;

      Input_HorizontalPosition.Name.Format(
          "Initial Horizontal Position From Left (1-%d)",
          (int)CHART_DRAWING_MAX_HORIZONTAL_AXIS_RELATIVE_POSITION);
      Input_HorizontalPosition.SetInt(20);
      Input_HorizontalPosition.SetIntLimits(
          1, (int)CHART_DRAWING_MAX_HORIZONTAL_AXIS_RELATIVE_POSITION);

      Input_VerticalPosition.Name.Format(
          "Initial Vertical Position From Bottom (1-%d)",
          (int)CHART_DRAWING_MAX_VERTICAL_AXIS_RELATIVE_POSITION);
      Input_VerticalPosition.SetInt(90);
      Input_VerticalPosition.SetIntLimits(
          1, (int)CHART_DRAWING_MAX_VERTICAL_AXIS_RELATIVE_POSITION);

      Input_DrawAboveMainPriceGraph.Name = "Draw Above Main Price Graph";
      Input_DrawAboveMainPriceGraph.SetYesNo(false);

      Input_UseBoldFont.Name = "Use Bold Font";
      Input_UseBoldFont.SetYesNo(true);

      Input_TextSize.Name = "Text Size";
      Input_TextSize.SetInt(14);
      Input_TextSize.SetIntLimits(3, 50);

      Input_TransparentLabelBackground.Name = "Transparent Label Background";
      Input_TransparentLabelBackground.SetYesNo(false);
    }
    else
    {
      auto ptr = (SecondaryPlugin *)sc.GetPersistentPointer(1);
      if (!ptr || ptr->port() != Port.GetInt())
      {
        delete ptr;
        ptr = new SecondaryPlugin(Host.GetString(), Port.GetInt());
        sc.SetPersistentPointer(1, ptr);
        sc.AddMessageToLog("Started client", 0);
      }
      s_SCPositionData position;
      if (ptr->gotFirstUpdate() && sc.GetTradePosition(position) > 0 &&
          !position.WorkingOrdersExist)
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
          int ret = 0;
          if (delta > 0)
          {
            ret = sc.BuyEntry(newOrder);
          }
          else if (delta < 0)
          {
            ret = sc.SellEntry(newOrder);
          }
          if (ret > 0)
          {
            BOOST_LOG_TRIVIAL(error) << "Order submission ignored";
          }
        }
      }

      SCString ConnectionInfo;
      auto serverBook = ptr->serverChartbook();
      auto timeOfLastMessage = ptr->timeOfLastMessage();
      auto timeSinceLastMessage =
          boost::posix_time::microsec_clock::local_time() - timeOfLastMessage;
      auto port = ptr->port();

      ConnectionInfo.Format("Connected to port %d book %s (ping: %d ms)", port,
                            serverBook.c_str(),
                            timeSinceLastMessage.total_milliseconds());

      if (timeSinceLastMessage >= boost::posix_time::time_duration(0, 0, 5) &&
          int(timeSinceLastMessage.total_seconds()) % 5 == 0)
      {
        sc.AddMessageToLog("Lost connection to server chartbook", 1);
      }

      int HorizontalPosition = Input_HorizontalPosition.GetInt();
      int VerticalPosition = Input_VerticalPosition.GetInt();

      int DrawAboveMainPriceGraph = Input_DrawAboveMainPriceGraph.GetYesNo();

      int TransparentLabelBackground =
          Input_TransparentLabelBackground.GetYesNo();
      int UseBoldFont = Input_UseBoldFont.GetYesNo();
      Subgraph_ConnectionInfo.LineWidth = Input_TextSize.GetInt();

      sc.AddAndManageSingleTextUserDrawnDrawingForStudy(
          sc, 0, HorizontalPosition, VerticalPosition, Subgraph_ConnectionInfo,
          TransparentLabelBackground, ConnectionInfo, DrawAboveMainPriceGraph,
          0, UseBoldFont);

      if (sc.LastCallToFunction)
      {
        auto ptr = (SecondaryPlugin *)sc.GetPersistentPointer(1);
        delete ptr;
        sc.SetPersistentPointer(1, nullptr);
      }
    }
  }
  catch (std::exception const &e)
  {
    sc.AddMessageToLog(e.what(), 1);
  }
  catch (...)
  {
    sc.AddMessageToLog("Unknown exception", 1);
  }
}
