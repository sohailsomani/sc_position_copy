#include "primary.hpp"
#include "boost/asio.hpp"
#include "sierrachart.h"

const char *hello_primary() { return "world"; }

SCDLLName("Position Copy Plugin for Primary Instance")

SCSFExport scsf_PrimaryInstance(SCStudyInterfaceRef sc) {
  if (sc.SetDefaults) {
    sc.GraphName = "Primary instance for position copying";
    sc.StudyDescription = "Relay position updates to clients";
    sc.GraphRegion = 0;
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;
    sc.AddMessageToLog("Testing, 123", 1);
    return;
  }
}
