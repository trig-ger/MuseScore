#ifndef _BEAT_TRACKER_H_
#define _BEAT_TRACKER_H_

#include "Event.h"

#include <vector>


class AgentParameters;

namespace BeatTracker {

      /** Perform beat tracking.
       *  @param events The onsets or peaks in a feature list
       *  @return The list of beats, or an empty list if beat tracking fails
       */
      std::vector<double> beatTrack(const EventList &events);

} // namespace BeatTracker


#endif

