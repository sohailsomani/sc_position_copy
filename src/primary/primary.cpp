#include "primary.hpp"
#include "boost/asio.hpp"
#include "sierrachart.h"
#include <map>

struct PrimaryPlugin
{
};

const char *hello_primary() { return "world"; }

SCDLLName("Position Copy Plugin for Primary Instance");

SCSFExport scsf_PrimaryInstance(SCStudyInterfaceRef sc)
{
  static std::set<s_sc *, std::shared_ptr<PrimaryPlugin>> s_instances;
  if (sc.SetDefaults)
  {
    sc.GraphName = "Primary instance for position copying";
    sc.StudyDescription = "Relay position updates to clients";
    sc.GraphRegion = 0;
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;
    sc.AddMessageToLog("Testing, 123", 1);
    return;
  }
}
