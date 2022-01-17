#include "primary.hpp"
#include "boost/asio.hpp"
#include "sierrachart.h"
#include <unordered_map>

SCDLLName("Position Copy Plugin for Primary Instance");

struct PrimaryPlugin
{
  explicit PrimaryPlugin(SCStudyInterfaceRef sc, unsigned int port)
      : m_sc(sc), m_port(port)
  {
    char buf[1024];
    snprintf(buf, sizeof(buf), "Creating new primary server on port %d",
             this->port());
    sc.AddMessageToLog(buf, 0);
  }
  ~PrimaryPlugin()
  {
    char buf[1024];
    snprintf(buf, sizeof(buf), "Stopping primary server on port %d",
             this->port());
    m_sc.AddMessageToLog(buf, 0);
  }

  unsigned int port() const { return m_port; }

private:
  SCStudyInterfaceRef m_sc;
  unsigned int m_port;
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

    return;
  }
  else if (sc.LastCallToFunction)
  {
    sc.AddMessageToLog("Last call", 0);
    auto it = s_instances.find(&sc);
    if (it != s_instances.end())
    {
      sc.AddMessageToLog("Stopping server", 0);
      s_instances.erase(it);
      it->second.reset();
    }
  }
  else
  {
    auto it = s_instances.find(&sc);
    if (it == s_instances.end() || (it->second->port() != Port.GetInt()))
    {
      auto it2 = s_instances.insert(std::make_pair(
          &sc, std::make_shared<PrimaryPlugin>(sc, Port.GetInt())));
    }
  }
}
