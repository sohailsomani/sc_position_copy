#include "secondary.hpp"
#include "sierrachart.h"

const char *hello_secondary() { return "world"; }

SCDLLName("Position Copy Plugin for Secondary Instance(s)");

SCSFExport scsf_SecondaryInstance(SCStudyInterfaceRef sc)
{
  if (sc.SetDefaults)
  {
    sc.GraphName = "Secondary instance for position copying";
    sc.StudyDescription =
        "Apply position updates received from primary to secondary";
    sc.GraphRegion = 0;
    sc.FreeDLL = 1;
    sc.AutoLoop = 0;
    sc.AddMessageToLog("Testing, 123", 1);
  }
}
