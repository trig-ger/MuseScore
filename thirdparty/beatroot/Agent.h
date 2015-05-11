#ifndef _AGENT_H_
#define _AGENT_H_

#include "Event.h"

#include <set>


struct AgentParameters
      {
                  /** The maximum amount by which a beat can be later than the
                   *  predicted beat time, expressed as a fraction of the beat
                   *  period.
                   */
      double postMarginFactor = 0.3;

                  /** The maximum amount by which a beat can be earlier than the
                   *  predicted beat time, expressed as a fraction of the beat
                   *  period.
                   */
      double preMarginFactor = 0.15;

                  /** The maximum allowed deviation from the initial tempo,
                   * expressed as a fraction of the initial beat period.
                   */
      double maxChange = 0.2;

                  /** The default value of expiryTime, which is the time (in
                   *  seconds) after which an Agent that has no Event matching its
                   *  beat predictions will be destroyed.
                   */
      double expiryTime = 10.0;
      };


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
      Agent(const AgentParameters &params, double ibi);

      bool operator<(const Agent &other) const;

                  /** The Agent's unique identity number. */
      int id;

                  /** Sum of salience values of the Events which have been
                   *  interpreted as beats by this Agent, weighted by their nearness
                   *  to the predicted beat times.
                   */
      double phaseScore;

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

                  /** The list of Events (onsets) accepted by this Agent as beats,
                   *  plus interpolated beats. */
      EventList events;

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
                  /** The default value of innerMargin, which is the maximum time
                   *  (in seconds) that a beat can deviate from the predicted beat
                   *  time without a fork occurring.
                   */
      static const double INNER_MARGIN;

                  /** The slope of the penalty function for onsets which do not
                   *  coincide precisely with predicted beat times.
                   */
      static const double CONF_FACTOR;

                  /** The reactiveness/inertia balance, i.e. degree of change in the
                   *  tempo, is controlled by the correctionFactor variable.  This
                   *  constant defines its default value, which currently is not
                   *  subsequently changed. The beat period is updated by the
                   *  reciprocal of the correctionFactor multiplied by the
                   *  difference between the predicted beat time and matching
                   *  onset.
                   */
      static const double DEFAULT_CORRECTION_FACTOR;

                  /** Controls the reactiveness/inertia balance, i.e. degree of
                   *  change in the tempo.  The beat period is updated by the
                   *  reciprocal of the correctionFactor multiplied by the
                   *  difference between the predicted beat time and matching
                   *  onset.
                   */
      double correctionFactor;
      };

#endif
