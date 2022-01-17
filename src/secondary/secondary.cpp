#include "secondary.hpp"
#include "boost/asio.hpp"
#include "boost/asio/deadline_timer.hpp"
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

SCDLLName("Position Copy Plugin for Secondary Instance");

namespace boost
{
void tss_cleanup_implemented() {}
} // namespace boost

using tcp = boost::asio::ip::tcp;
using boost::asio::io_service;

struct SecondaryPlugin
{
  explicit SecondaryPlugin(unsigned int port)
      : m_position(0), m_port(port), m_work(m_service)
  {
  }

  ~SecondaryPlugin() {}

  unsigned int port() const { return m_port; }

private:
  t_OrderQuantity32_64 m_position;
  unsigned int m_port;
  boost::asio::io_service m_service;
  boost::asio::io_service::work m_work;
  tcp::endpoint m_endpoint;
};

const char *hello_secondary() { return "world"; }

static std::unordered_map<s_sc *, std::unique_ptr<SecondaryPlugin>> s_instances;

SCSFExport scsf_SecondaryInstance(SCStudyInterfaceRef sc)
{
  SCInputRef Port = sc.Input[0];

  if (sc.SetDefaults)
  {
    sc.GraphName = "Secondary instance for position copying";
    sc.StudyDescription = "Relay position updates to clients";
    sc.GraphRegion = 0;
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;

    Port.Name = "Primary instance port";
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
      sc.AddMessageToLog("Stopping client", 0);
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
                   &sc, std::make_unique<SecondaryPlugin>(Port.GetInt())))
               .first;
      sc.AddMessageToLog("Started client", 0);
    }
  }
}
