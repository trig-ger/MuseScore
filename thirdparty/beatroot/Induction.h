#ifndef _INDUCTION_H_
#define _INDUCTION_H_

#include "Event.h"

#include <set>


class Agent;
struct AgentParameters;

/** Performs tempo induction by finding clusters of similar
 *  inter-onset intervals (IOIs), ranking them according to the number
 *  of intervals and relationships between them, and returning a set
 *  of tempo hypotheses for initialising the beat tracking agents.
 */
namespace Induction
{

/** Performs tempo induction (see JNMR 2001 paper by Simon Dixon for details).
 *  @param events The onsets (or other events) from which the tempo is induced
 *  @return A list of beat tracking agents, where each is initialised with one
 *          of the top tempo hypotheses but no beats
 */
std::vector<Agent> doBeatInduction(const AgentParameters &params, const EventList &events);

}


#endif
