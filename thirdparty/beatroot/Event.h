#ifndef _EVENT_H_
#define _EVENT_H_

#include <vector>


struct Event
      {
      double time;
      double salience;

      Event()
            : time(0), salience(0)
            {}
      Event(double t, double s)
            : time(t), salience(s)
            {}

      bool operator<(const Event &e) const
            {
            return time < e.time;
            }
      bool operator!=(const Event &e) const
            {
            return time != e.time || salience != e.salience;
            }
      };

typedef std::vector<Event> EventList;


#endif

