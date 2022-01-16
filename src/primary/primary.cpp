#include "primary.hpp"
#include "boost/asio.hpp"
#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/dispatch.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/asio/write.hpp"
#include "boost/date_time/gregorian/formatters.hpp"
#include "boost/date_time/posix_time/time_formatters.hpp"
#include "boost/json/object.hpp"
#include "boost/json/serialize.hpp"
#include "boost/log/trivial.hpp"
#include "boost/system/detail/errc.hpp"
#include "boost/system/is_error_code_enum.hpp"
#include "sierrachart.h"
#include <mutex>
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
  explicit Connection(tcp::socket socket) : m_socket(std::move(socket))
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
  }

  tcp::socket &socket() { return m_socket; }

private:
  tcp::socket m_socket;
};

struct PrimaryPlugin
{
  explicit PrimaryPlugin(std::string chartbookName, unsigned int port)
      : m_chartbookName(chartbookName), m_port(port), m_work(m_service),
        m_endpoint(tcp::v4(), port), m_acceptor(m_service, m_endpoint),
        m_thread(std::bind(&PrimaryPlugin::threadFunc, this)),
        m_timer(m_service)
  {
    BOOST_LOG_TRIVIAL(info)
        << "Creating new primary server on port " << this->port();
    sendPing();
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

  void processPosition(t_OrderQuantity32_64 position)
  {
    if (position != m_position)
    {
      m_position = position;
      sendPosition();
    }
  }

  unsigned int numClients() const { return m_connections.size(); }

private:
  void sendMessage(boost::json::object msg)
  {
    msg["cb"] = m_chartbookName;
    const auto json = boost::json::serialize(msg);
    for (auto &conn : m_connections)
    {
      if (conn->socket().is_open())
      {
        boost::asio::async_write(conn->socket(), boost::asio::buffer(json),
                                 makeCompletionHandler(*conn));
      }
    }
  }

  void sendPosition() { sendMessage({{"position", m_position}}); }

  void sendPing()
  {
    auto end = std::remove_if(m_connections.begin(), m_connections.end(),
                              [](std::unique_ptr<Connection> &conn) {
                                return !conn->socket().is_open();
                              });
    m_connections.erase(end, m_connections.end());
    auto tick = boost::posix_time::second_clock::local_time();
    if (tick.time_of_day().seconds() % 5 == 0)
    {
      BOOST_LOG_TRIVIAL(info) << m_connections.size() << " clients connected";
    }
    boost::json::object msg = {
        {"ping", boost::posix_time::to_iso_string(tick)},
    };

    sendMessage(msg);

    m_timer.expires_after(boost::asio::chrono::seconds(1));
    m_timer.async_wait(
        [this](const boost::system::error_code &) { sendPing(); });
  }

  std::function<void(boost::system::error_code const &ec, std::size_t)>
  makeCompletionHandler(Connection &conn)
  {
    return [&conn](boost::system::error_code const &ec, std::size_t) {
      if (ec)
      {
        BOOST_LOG_TRIVIAL(error) << "Error, closing socket: " << ec;
        boost::system::error_code ec2;
        conn.socket().close(ec2);
      }
    };
  }

  void accept()
  {
    m_acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec)
          {
            m_connections.push_back(
                std::make_unique<Connection>(std::move(socket)));
          }
          sendPosition();
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

  t_OrderQuantity32_64 m_position = 0;
  std::string m_chartbookName;
  unsigned int m_port;
  boost::asio::io_service m_service;
  boost::asio::io_service::work m_work;
  tcp::endpoint m_endpoint;
  tcp::acceptor m_acceptor;
  std::vector<std::unique_ptr<Connection>> m_connections;
  std::thread m_thread;
  boost::asio::steady_timer m_timer;
};

const char *hello_primary() { return "world"; }

SCSFExport scsf_PrimaryInstance(SCStudyInterfaceRef sc)
{

  SCSubgraphRef Subgraph_ServerInfo = sc.Subgraph[0];

  SCInputRef Port = sc.Input[0];
  SCInputRef Input_HorizontalPosition = sc.Input[1];
  SCInputRef Input_VerticalPosition = sc.Input[2];
  SCInputRef Input_DrawAboveMainPriceGraph = sc.Input[3];
  SCInputRef Input_UseBoldFont = sc.Input[4];
  SCInputRef Input_TransparentLabelBackground = sc.Input[5];
  SCInputRef Input_TextSize = sc.Input[6];

  try
  {
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

      Subgraph_ServerInfo.Name = "Server info";
      Subgraph_ServerInfo.LineWidth = 20;
      Subgraph_ServerInfo.DrawStyle = DRAWSTYLE_CUSTOM_TEXT;
      Subgraph_ServerInfo.PrimaryColor = RGB(0, 0, 0);       // black
      Subgraph_ServerInfo.SecondaryColor = RGB(255, 127, 0); // Orange
      Subgraph_ServerInfo.SecondaryColorUsed = true;
      Subgraph_ServerInfo.DisplayNameValueInWindowsFlags = 1;

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
      auto ptr = (PrimaryPlugin *)sc.GetPersistentPointer(1);
      if (!ptr || ptr->port() != Port.GetInt())
      {
        delete ptr;
        ptr = new PrimaryPlugin(sc.ChartbookName().GetChars(), Port.GetInt());
        sc.SetPersistentPointer(1, ptr);
        sc.AddMessageToLog("Started server", 0);
      }
      s_SCPositionData position;
      sc.GetTradePosition(position);
      ptr->processPosition(position.PositionQuantity);

      SCString ServerInfo;
      ServerInfo.Format("Port: %d NumClients: %d", ptr->port(),
                        ptr->numClients());

      int HorizontalPosition = Input_HorizontalPosition.GetInt();
      int VerticalPosition = Input_VerticalPosition.GetInt();

      int DrawAboveMainPriceGraph = Input_DrawAboveMainPriceGraph.GetYesNo();

      int TransparentLabelBackground =
          Input_TransparentLabelBackground.GetYesNo();
      int UseBoldFont = Input_UseBoldFont.GetYesNo();
      Subgraph_ServerInfo.LineWidth = Input_TextSize.GetInt();

      sc.AddAndManageSingleTextUserDrawnDrawingForStudy(
          sc, 0, HorizontalPosition, VerticalPosition, Subgraph_ServerInfo,
          TransparentLabelBackground, ServerInfo, DrawAboveMainPriceGraph, 0,
          UseBoldFont);

      if (sc.LastCallToFunction)
      {
        auto ptr = (PrimaryPlugin *)sc.GetPersistentPointer(1);
        delete ptr;
        sc.SetPersistentPointer(1, nullptr);
      }
    }
  }
  catch (std::exception const &e)
  {
    sc.AddMessageToLog(e.what(), 1);
    BOOST_LOG_TRIVIAL(error) << e.what();
  }
}
