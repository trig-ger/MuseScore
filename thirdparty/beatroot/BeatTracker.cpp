#include "BeatTracker.h"
#include "Agent.h"
#include "AgentList.h"
#include "Induction.h"


namespace BeatTracker {

std::vector<double> beatTrack(const EventList &events)
      {
      AgentList agents = Induction::doBeatInduction(AgentParameters(), events);
      agents.beatTrack(events, AgentParameters(), -1);

      Agent *best = agents.bestAgent();
      std::vector<double> resultBeatTimes;

      if (best) {
            best->fillBeats(-1.0);  // -1.0 means from the very beginning
            for (const auto &e: best->events)
                  resultBeatTimes.push_back(e.time);
            }
      return resultBeatTimes;
      }

} // namespace BeatTracker


