#include "Agent.h"
#include "Event.h"

#include <cmath>
#include <algorithm>


namespace BeatTracker {

const double Agent::INNER_MARGIN = 0.040;
const double Agent::CONF_FACTOR = 0.5;
const double Agent::DEFAULT_CORRECTION_FACTOR = 50.0;


Agent::Agent(double interBeatInterval)
    : phaseScore(0.0)
    , beatCount(0)
    , beatInterval(interBeatInterval)
    , initialBeatInterval(interBeatInterval)
    , beatTime(-1.0)
    , expiryTime(AgentParameters().expiryTime)
    , maxChange(AgentParameters().maxChange)
    , preMargin(interBeatInterval * AgentParameters().preMarginFactor)
    , postMargin(interBeatInterval * AgentParameters().postMarginFactor)
    , innerMargin(INNER_MARGIN)
    , correctionFactor(DEFAULT_CORRECTION_FACTOR)
{
    id = generateNewId();
}

int Agent::generateNewId()
{
    static int idCounter = 0;
    return idCounter++;
}

Agent Agent::newAgentFromGiven(const Agent &agent)
{
      Agent a(agent);
      a.id = generateNewId();
      return a;
}

bool Agent::operator<(const Agent &other) const
{
    if (beatInterval == other.beatInterval)
        return id < other.id;      // ensure stable ordering
    return beatInterval < other.beatInterval;
}

void Agent::acceptEvent(const Event &e, double err, int beats)
{
    events.push_back(e);
    beatTime = e.time;

    if (std::fabs(initialBeatInterval - beatInterval - err / correctionFactor)
            < maxChange * initialBeatInterval) {
        beatInterval += err / correctionFactor;         // adjust tempo
    }
    beatCount += beats;
    const double conFactor = 1.0 - CONF_FACTOR * err / (err > 0 ? postMargin: -preMargin);
    phaseScore += conFactor * e.salience;
}

} // namespace BeatTracker
