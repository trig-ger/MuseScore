#ifndef _EVENT_H_
#define _EVENT_H_


namespace BeatTracker {

struct Event
      {
      Event() : time(0), salience(0)
            {}
      Event(double t, double s) : time(t), salience(s)
            {}

      bool operator<(const Event &e) const
            {
            return time < e.time;
            }
      bool operator==(const Event &e) const
            {
            return time == e.time && salience == e.salience;
            }
      bool operator!=(const Event &e) const
            {
            return !operator==(e);
            }

      double time;
      double salience;
      };

} // namespace BeatTracker


#endif

