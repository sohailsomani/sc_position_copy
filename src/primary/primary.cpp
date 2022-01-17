#include "primary.hpp"
#include "boost/asio.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/log/trivial.hpp"
#include "sierrachart.h"
#include <thread>
#include <unordered_map>

SCDLLName("Position Copy Plugin for Primary Instance");

namespace boost
{
void tss_cleanup_implemented() {}
} // namespace boost

struct PrimaryPlugin
{
  explicit PrimaryPlugin(unsigned int port)
      : m_port(port), m_work(m_service),
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

  void getLogs(std::vector<std::string> &out) { out.clear(); }

private:
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

  void addLog(std::string) {}

  unsigned int m_port;
  boost::asio::io_service m_service;
  boost::asio::io_service::work m_work;
  std::thread m_thread;
};

const char *hello_primary() { return "world"; }

static std::unordered_map<s_sc *, std::shared_ptr<PrimaryPlugin>> s_instances;

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
      auto ptr = it->second;
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
                   &sc, std::make_shared<PrimaryPlugin>(Port.GetInt())))
               .first;
      sc.AddMessageToLog("Started server", 0);
    }
  }
}
