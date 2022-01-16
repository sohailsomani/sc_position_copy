#include "primary.hpp"
#include "sierrachart.h"

const char *hello_primary() { return "world"; }

SCDLLName("Position Copy Plugin for Primary Instance")

SCSFExport scsf_TestingPrimary(SCStudyInterfaceRef sc) {
  if (sc.SetDefaults) {
    sc.AddMessageToLog("Testing, 123", 1);
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;
    return;
  }
}
