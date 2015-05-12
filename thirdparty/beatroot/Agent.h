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
      Agent(double interBeatInterval);
      Agent& operator=(const Agent &) = delete;
      Agent(Agent &&) = default;
      Agent& operator=(Agent &&) = default;

      bool operator<(const Agent &other) const;

      const std::vector<Event>& events() const { return events_; }
      std::vector<Event>& events() { return events_; }

      double phaseScore() const { return phaseScore_; }
      double beatCount() const { return beatCount_; }
      double beatInterval() const { return beatInterval_; }
      double beatTime() const { return beatTime_; }
      double preMargin() const { return preMargin_; }
      double postMargin() const { return postMargin_; }

      static double innerMargin();
      static double expiryTime();

      void markForDeletion();
      bool isMarkedForDeletion() const { return isMarkedForDeletion_; }

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

                  /** Sum of salience values of the Events which have been
                   *  interpreted as beats by this Agent, weighted by their nearness
                   *  to the predicted beat times.
                   */
      double phaseScore_;

                  /** The number of beats found by this Agent, including
                   *  interpolated beats.
                   */
      int beatCount_;

                  /** The current tempo hypothesis of the Agent, expressed as the
                   *  beat period in seconds.
                   */
      double beatInterval_;

                  /** The initial tempo hypothesis of the Agent, expressed as the
                   *  beat period in seconds.
                   */
      double initialBeatInterval_;

                  /** The time of the most recent beat accepted by this Agent. */
      double beatTime_;

                  /** The size of the outer half-window before the predicted beat time. */
      double preMargin_;

                  /** The size of the outer half-window after the predicted beat time. */
      double postMargin_;

      bool isMarkedForDeletion_;

                  /** The list of Events (onsets) accepted by this Agent as beats,
                   *  plus interpolated beats. */
      std::vector<Event> events_;
      };

} // namespace BeatTracker

#endif
