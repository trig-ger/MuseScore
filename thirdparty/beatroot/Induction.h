#ifndef _INDUCTION_H_
#define _INDUCTION_H_

#include <map>
#include <vector>


class Agent;
struct Event;

namespace BeatTracker
{

class BeatMap
      {
   public:
      BeatMap(size_t beginEventIndex, size_t endEventIndex)
            : beginShift_(beginEventIndex)
            , beatsForEvents_(endEventIndex - beginEventIndex)
            {
            }

      void addBeats(size_t eventIndex, std::vector<double> &&beats)
            {
            beatsForEvents_[eventIndex - beginShift_]= beats;
            }

      const std::vector<double>& beatsForEvent(size_t eventIndex) const
            {
            if (eventIndex <= beginShift_)
                  return beatsForEvents_.front();
            if (eventIndex >= beginShift_ + beatsForEvents_.size())
                  return beatsForEvents_.back();
            return beatsForEvents_[eventIndex - beginShift_];
            }

      bool isEmpty() const { return beatsForEvents_.empty(); }

   private:
      // all earlier events than the event with this index
      // have the same beat array
      size_t beginShift_;
      // for every event it stores an array of beats
      std::vector<std::vector<double>> beatsForEvents_;
      };

/** Performs tempo induction by finding clusters of similar
 *  inter-onset intervals (IOIs), ranking them according to the number
 *  of intervals and relationships between them, and returning a set
 *  of tempo hypotheses for initialising the beat tracking agents.
 *
 *  @param events The onsets (or other events) from which the tempo is induced
 *  @return A list of beat tracking agents, where each is initialised with one
 *          of the top tempo hypotheses but no beats
 */
BeatMap doBeatInduction(const std::vector<Event> &events);

}


#endif
