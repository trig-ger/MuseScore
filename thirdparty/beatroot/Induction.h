#ifndef _INDUCTION_H_
#define _INDUCTION_H_

#include <vector>


class Agent;
struct Event;

namespace BeatTracker
{

/** Performs tempo induction by finding clusters of similar
 *  inter-onset intervals (IOIs), ranking them according to the number
 *  of intervals and relationships between them, and returning a set
 *  of tempo hypotheses for initialising the beat tracking agents.
 *
 *  @param events The onsets (or other events) from which the tempo is induced
 *  @return A list of beat tracking agents, where each is initialised with one
 *          of the top tempo hypotheses but no beats
 */
std::vector<Agent> doBeatInduction(const std::vector<Event> &events);

}


#endif
