#include "BeatTracker.h"
#include "Agent.h"
#include "Induction.h"

#include <cmath>
#include <algorithm>

#include <QtGlobal>


namespace BeatTracker {
namespace {

/** Flag for choice between sum and average beat salience values for Agent scores.
 *  The use of summed saliences favours faster tempi or lower metrical levels. */
const bool useAverageSalience = false;

/** For the purpose of removing duplicate agents, the default JND of IBI */
const double DEFAULT_BI = 0.02;

/** For the purpose of removing duplicate agents, the default JND of phase */
const double DEFAULT_BT = 0.04;


/** Removes Agents from the list which are duplicates of other Agents.
 *  A duplicate is defined by the tempo and phase thresholds
 *  thresholdBI and thresholdBT respectively.
 */
void removeDuplicateAgents(std::vector<Agent> &list)
{
    if (list.size() <= 1)
        return;

    std::vector<size_t> removed(list.size(), 0);

    for (auto it1 = list.begin(); it1 != std::prev(list.end()); ) {
        const auto curIt1 = it1++;
        for (auto it2 = std::next(it1); it2 != list.end(); ) {
            const auto curIt2 = it2++;
            if (curIt2->beatInterval - curIt1->beatInterval > DEFAULT_BI)
                break;          // next intervals will be only larger
            if (std::fabs(curIt1->beatTime - curIt2->beatTime) > DEFAULT_BT)
                continue;
                    // remove the agent with the lowest score among two
            if (curIt1->phaseScore < curIt2->phaseScore) {
                removed[curIt1 - list.begin()] = 1;
                break;
            }
            else {
                removed[curIt2 - list.begin()] = 1;
            }
        }
    }

    std::vector<Agent> newList;
    for (size_t i = 0; i != list.size(); ++i) {
          if (removed[i] == 0)
                newList.push_back(list[i]);
          }
    std::swap(newList, list);
}

/** The given Event is tested for a possible beat time.
 *  The following situations can occur:
 *  1) The Agent has no beats yet; the Event is accepted as the first beat.
 *  2) The Event is beyond expiryTime seconds after the Agent's last
 *     'confirming' beat; the Agent is terminated.
 *  3) The Event is within the innerMargin of the beat prediction;
 *     it is accepted as a beat.
 *  4) The Event is within the postMargin's of the beat prediction;
 *     it is accepted as a beat by this Agent,
 *     and a new Agent is created which doesn't accept it as a beat.
 *  5) The Event is ignored because it is outside the windows around
 *     the Agent's predicted beat time.
 * @param e The Event to be tested
 * @param agentList The list of all agents,
 *        which is updated if a new agent is created.
 * @return Indicate whether the given Event was accepted as a beat by this Agent.
 */
bool considerAsBeat(const Agent &agent,
                    std::vector<Agent> &agentList,
                    const Event &e)
{
    if (agent.beatTime < 0) {     // first event
        Agent newAgent(agent);
        newAgent.acceptEvent(e, 0.0, 1);
        agentList.push_back(newAgent);
        return true;
    }
            // subsequent events
    if (e.time - std::prev(agent.events.end())->time > agent.expiryTime)
        return false;       // and don't insert agent, i.e. remove it

    const int beats = nearbyint((e.time - agent.beatTime) / agent.beatInterval);
    const double err = e.time - agent.beatTime - beats * agent.beatInterval;

    if (beats > 0 && -agent.preMargin <= err && err <= agent.postMargin) {
        if (std::fabs(err) > agent.innerMargin) {
                    // Create new agent that skips this event
                    //   (avoids large phase jump)
            Agent newAgent(agent);
            newAgent.id = Agent::generateNewId();
            agentList.push_back(newAgent);
        }
        Agent newAgent(agent);
        newAgent.acceptEvent(e, err, beats);
        agentList.push_back(newAgent);
        return true;
    }

    agentList.push_back(agent);

    return false;
}

/** Perform beat tracking on a list of events (onsets).
 *  @param el The list of onsets (or events or peaks) to beat track.
 *  @param stop Do not find beats after <code>stop</code> seconds.
 */
void beatTrack(const EventList &eventList, const AgentParameters &params,
               std::vector<Agent> &agentList, double stopTime)
{
            // if given for one, assume given for others
    const bool isPhaseGiven = (!agentList.empty() && agentList.begin()->beatTime >= 0);

    for (const Event &ev: eventList) {
        if (stopTime > 0 && ev.time > stopTime)
            break;

        bool created = isPhaseGiven;
        double prevBeatInterval = -1.0;
        std::vector<Agent> newAgentList;

                // agents are sorted by beat interval in ascending order
        for (auto it = agentList.begin(); it != agentList.end(); ++it) {
            const Agent &a = *it;
            if (a.beatInterval != prevBeatInterval) {
                if (prevBeatInterval >= 0 && !created && ev.time < 5.0) {
                            // Create a new agent with a different phase
                    considerAsBeat(Agent(params, prevBeatInterval), newAgentList, ev);
                }
                prevBeatInterval = a.beatInterval;
                created = isPhaseGiven;
            }
            if (considerAsBeat(a, newAgentList, ev))
                created = true;
        }

        std::sort(newAgentList.begin(), newAgentList.end());
        std::swap(agentList, newAgentList);
        removeDuplicateAgents(agentList);
    }
}

/** Finds the Agent with the highest score in the list,
 *  or NULL if beat tracking has failed.
 *  @return The Agent with the highest score
 */
std::vector<Agent>::const_iterator findBestAgent(const std::vector<Agent> &agentList)
{
    double best = -1.0;
    auto bestIt = agentList.end();

    for (auto it = agentList.begin(); it != agentList.end(); ++it) {
        if (it->events.empty())
            continue;
        const double conf = it->phaseScore / (useAverageSalience ? it->beatCount : 1.0);
        if (conf > best) {
            bestIt = it;
            best = conf;
        }
    }
    return bestIt;
}

/** Interpolates missing beats in the Agent's beat track.
 *  @param start Ignore beats earlier than this start time
 */
void interpolateBeats(Agent &agent, double start)
{
    if (agent.events.empty())
        return;

    EventList newEvents;
    auto it = agent.events.begin();
    double prevBeat = it->time;

    for (++it; it != agent.events.end(); ++it) {
        double nextBeat = it->time;
                // prefer slow tempo
        int beats = nearbyint((nextBeat - prevBeat) / agent.beatInterval - 0.01);
        double currentInterval = (nextBeat - prevBeat) / beats;
        for ( ; nextBeat > start && beats > 1; --beats) {
            prevBeat += currentInterval;
            newEvents.push_back(Event(prevBeat, 0));
        }
        prevBeat = nextBeat;
    }

    agent.events.insert(agent.events.end(), newEvents.begin(), newEvents.end());
    std::sort(agent.events.begin(), agent.events.end());
}

} // namespace

std::vector<double> beatTrack(const EventList &events)
      {
      std::vector<Agent> agents = Induction::doBeatInduction(AgentParameters(), events);
      beatTrack(events, AgentParameters(), agents, -1);

      const auto bestAgentIt = findBestAgent(agents);
      std::vector<double> resultBeatTimes;

      if (bestAgentIt != agents.end()) {
            Agent bestAgent = *bestAgentIt;
            interpolateBeats(bestAgent, -1.0);  // -1.0 means from the very beginning
            for (const auto &e: bestAgent.events)
                  resultBeatTimes.push_back(e.time);
            }
      return resultBeatTimes;
      }

} // namespace BeatTracker
