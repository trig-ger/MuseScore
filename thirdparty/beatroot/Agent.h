#ifndef _AGENT_H_
#define _AGENT_H_


#include <vector>


namespace BeatTracker {

struct Event;

/** Agent is the central class for beat tracking.
 *  Each Agent object has a tempo hypothesis, a history of tracked beats, and
 *  a score evaluating the continuity, regularity and salience of its beat track.
 */
class Agent
      {
   public:
          /** Constructor: the work is performed by init()
           *  @param ibi The beat period (inter-beat interval)
           *  of the Agent's tempo hypothesis.
           */
      Agent(double interBeatInterval);
      Agent& operator=(const Agent &) = delete;
      Agent(Agent &&) = default;
      Agent& operator=(Agent &&) = default;

      bool operator<(const Agent &other) const;

      const std::vector<Event>& events() const { return events_; }
      std::vector<Event>& events() { return events_; }

      double phaseScore() const { return phaseScore_; }

      void markForDeletion();
      bool isMarkedForDeletion() const { return isMarkedForDeletion_; }

                  /** The number of beats found by this Agent, including
                   *  interpolated beats.
                   */
      int beatCount;

                  /** The current tempo hypothesis of the Agent, expressed as the
                   *  beat period in seconds.
                   */
      double beatInterval;

                  /** The initial tempo hypothesis of the Agent, expressed as the
                   *  beat period in seconds.
                   */
      double initialBeatInterval;

                  /** The time of the most recent beat accepted by this Agent. */
      double beatTime;

                  /** The time (in seconds) after which an Agent that has no Event
                   *  matching its beat predictions will be destroyed.
                   */
      double expiryTime;

                  /** The maximum allowed deviation from the initial tempo,
                   *  expressed as a fraction of the initial beat period.
                   */
      double maxChange;

                  /** The size of the outer half-window before the predicted beat time. */
      double preMargin;

                  /** The size of the outer half-window after the predicted beat time. */
      double postMargin;

                  /** The maximum time (in seconds) that a beat can deviate from the
                   *  predicted beat time without a fork occurring (i.e. a 2nd Agent
                   *  being created).
                   */
      double innerMargin;

                  /** Accept a new Event as a beat time, and update the state of the Agent accordingly.
                   *  @param e The Event which is accepted as being on the beat.
                   *  @param err The difference between the predicted and actual beat times.
                   *  @param beats The number of beats since the last beat that matched an Event.
                   */
      void acceptEvent(const Event &e, double err, int beats);

                  /** Generate the identity number of the next created Agent */
      static int generateNewId();

      static Agent newAgentFromGiven(const Agent &agent);

   private:
      Agent(const Agent &) = default;

                  /** The Agent's unique identity number. */
      int id_;

                  /** Controls the reactiveness/inertia balance, i.e. degree of
                   *  change in the tempo.  The beat period is updated by the
                   *  reciprocal of the correctionFactor multiplied by the
                   *  difference between the predicted beat time and matching
                   *  onset.
                   */
      double correctionFactor_;

                  /** Sum of salience values of the Events which have been
                   *  interpreted as beats by this Agent, weighted by their nearness
                   *  to the predicted beat times.
                   */
      double phaseScore_;

      bool isMarkedForDeletion_;

                  /** The list of Events (onsets) accepted by this Agent as beats,
                   *  plus interpolated beats. */
      std::vector<Event> events_;
      };

} // namespace BeatTracker

#endif
