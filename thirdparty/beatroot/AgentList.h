/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
  Vamp feature extraction plugin for the BeatRoot beat tracker.

  Centre for Digital Music, Queen Mary, University of London.
  This file copyright 2011 Simon Dixon, Chris Cannam and QMUL.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.  See the file
  COPYING included with this distribution for more information.
*/

#ifndef _AGENT_LIST_H_
#define _AGENT_LIST_H_

#include "Event.h"

#include <vector>
#include <cstddef>


class Agent;
class AgentParameters;


      /** Class for maintaining the set of all Agents involved in beat tracking a piece of music.
       */
class AgentList
      {
   public:
      ~AgentList();

      bool empty() const { return list.empty(); }
      std::vector<Agent>::iterator begin() { return list.begin(); }
      std::vector<Agent>::iterator end() { return list.end(); }
      size_t size() { return list.size(); }

      void push_back(const Agent &a) { list.push_back(a); }

                  /** Flag for choice between sum and average beat salience values for Agent scores.
                   *  The use of summed saliences favours faster tempi or lower metrical levels. */
      static bool useAverageSalience;

                  /** For the purpose of removing duplicate agents, the default JND of IBI */
      static const double DEFAULT_BI;

                  /** For the purpose of removing duplicate agents, the default JND of phase */
      static const double DEFAULT_BT;

                  /** Inserts newAgent into the list in ascending order of beatInterval */
      void add(const Agent &newAgent);

                  /** Perform beat tracking on a list of events (onsets).
                   *  @param el The list of onsets (or events or peaks) to beat track
                   */
      void beatTrack(const EventList &el, const AgentParameters &params)
            {
            beatTrack(el, params, -1.0);
            }
                  /** Perform beat tracking on a list of events (onsets).
                   *  @param el The list of onsets (or events or peaks) to beat track.
                   *  @param stop Do not find beats after <code>stop</code> seconds.
                   */
      void beatTrack(const EventList &el, const AgentParameters &params, double stop);

                  /** Finds the Agent with the highest score in the list,
                   *  or NULL if beat tracking has failed.
                   *  @return The Agent with the highest score
                   */
      Agent *bestAgent();

   private:
          /** Sorts the AgentList by increasing beatInterval. */
      void sort();

          /** Removes Agents from the list which are duplicates of other Agents.
           *  A duplicate is defined by the tempo and phase thresholds
           *  thresholdBI and thresholdBT respectively.
           */
      void removeDuplicates();

      std::vector<Agent> list;
      };


#endif

