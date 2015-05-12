#include "Agent.h"
#include "Event.h"

#include <cmath>
#include <algorithm>


namespace BeatTracker {
namespace {

/** The maximum amount by which a beat can be earlier than the
 *  predicted beat time, expressed as a fraction of the beat
 *  period.
 */
const double PRE_MARGIN_FACTOR = 0.15;

/** The maximum amount by which a beat can be later than the
 *  predicted beat time, expressed as a fraction of the beat
 *  period.
 */
const double POST_MARGIN_FACTOR = 0.3;

/** The maximum allowed deviation from the initial tempo,
 * expressed as a fraction of the initial beat period.
 */
const double MAX_TEMPO_CHANGE = 0.2;

/** The default value of expiryTime, which is the time (in
 *  seconds) after which an Agent that has no Event matching its
 *  beat predictions will be destroyed.
 */
const double EXPIRY_TIME = 10.0;

/** The default value of innerMargin, which is the maximum time
 *  (in seconds) that a beat can deviate from the predicted beat
 *  time without a fork occurring.
 */
const double INNER_MARGIN = 0.040;

/** The slope of the penalty function for onsets which do not
 *  coincide precisely with predicted beat times.
 */
const double CONF_FACTOR = 0.5;

/** The reactiveness/inertia balance, i.e. degree of change in the
 *  tempo, is controlled by the correction factor variable.  This
 *  constant defines its default value, which currently is not
 *  subsequently changed. The beat period is updated by the
 *  reciprocal of the correctionFactor multiplied by the
 *  difference between the predicted beat time and matching
 *  onset.
 */
const double DEFAULT_CORRECTION_FACTOR = 50.0;

} // namespace


Agent::Agent(double interBeatInterval)
    : beatTime(-1.0)
    , expiryTime(EXPIRY_TIME)
    , maxChange(MAX_TEMPO_CHANGE)
    , preMargin(interBeatInterval * PRE_MARGIN_FACTOR)
    , postMargin(interBeatInterval * POST_MARGIN_FACTOR)
    , innerMargin(INNER_MARGIN)
    , correctionFactor_(DEFAULT_CORRECTION_FACTOR)
    , phaseScore_(0.0)
    , beatCount_(0)
    , beatInterval_(interBeatInterval)
    , initialBeatInterval_(interBeatInterval)
    , isMarkedForDeletion_(false)
{
    id_ = generateNewId();
}

int Agent::generateNewId()
{
    static int idCounter = 0;
    return idCounter++;
}

Agent Agent::newAgentFromGiven(const Agent &agent)
{
      Agent a(agent);
      a.id_ = generateNewId();
      return a;
}

bool Agent::operator<(const Agent &other) const
{
    if (beatInterval_ == other.beatInterval_)
        return id_ < other.id_;      // ensure stable ordering
    return beatInterval_ < other.beatInterval_;
}

void Agent::markForDeletion()
{
      isMarkedForDeletion_ = true;
}

void Agent::acceptEvent(const Event &e, double err, int beats)
{
    events_.push_back(e);
    beatTime = e.time;

    if (std::fabs(initialBeatInterval_ - beatInterval_ - err / correctionFactor_)
            < maxChange * initialBeatInterval_) {
        beatInterval_ += err / correctionFactor_;         // adjust tempo
    }
    beatCount_ += beats;
    const double conFactor = 1.0 - CONF_FACTOR * err / (err > 0 ? postMargin: -preMargin);
    phaseScore_ += conFactor * e.salience;
}

} // namespace BeatTracker
