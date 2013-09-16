#ifndef IMPORTMIDI_TUPLET_H
#define IMPORTMIDI_TUPLET_H

#include "importmidi_fraction.h"


namespace Ms {

class MidiChord;
class DurationElement;
class TimeSigMap;
class MTrack;
class Staff;

namespace MidiTuplet {

struct TupletInfo
      {
      ReducedFraction onTime = {-1, 1};  // invalid
      ReducedFraction len = {-1, 1};
      int tupletNumber = -1;
      ReducedFraction tupletQuant;
      ReducedFraction regularQuant;
                  // <chord onTime, chord iterator>
      std::map<ReducedFraction, std::multimap<ReducedFraction, MidiChord>::iterator> chords;
      ReducedFraction tupletSumError;
      ReducedFraction regularSumError;
      ReducedFraction sumLengthOfRests;
      int firstChordIndex = -1;
      std::multimap<ReducedFraction, MidiChord>::iterator tiedChord;
      };

struct TupletData
      {
      int voice;
      ReducedFraction onTime;
      ReducedFraction len;
      int tupletNumber;
      ReducedFraction tupletQuant;
      std::vector<DurationElement *> elements;
      };

// conversion ratios from tuplet durations to regular durations
// for example, 8th note in triplet * 3/2 = regular 8th note

const std::map<int, ReducedFraction> &tupletRatios();

ReducedFraction findOffTimeRaster(const ReducedFraction &noteOffTime,
                                  int voice,
                                  const ReducedFraction &regularQuant,
                                  const std::vector<TupletInfo> &tuplets);

ReducedFraction findOffTimeRaster(const ReducedFraction &noteOffTime,
                                  int voice,
                                  const ReducedFraction &regularQuant,
                                  const std::multimap<ReducedFraction, TupletData> &tupletEvents);

std::vector<TupletData>
findTupletsInBarForDuration(int voice,
                            const ReducedFraction &barStartTick,
                            const ReducedFraction &durationOnTime,
                            const ReducedFraction &durationLen,
                            const std::multimap<ReducedFraction, TupletData> &tupletEvents);

std::multimap<ReducedFraction, TupletData>::const_iterator
findTupletForTimeRange(int voice,
                       const ReducedFraction &onTime,
                       const ReducedFraction &len,
                       const std::multimap<ReducedFraction, TupletData> &tupletEvents);

std::multimap<ReducedFraction, TupletData>::const_iterator
findTupletContainsTime(int voice,
                       const ReducedFraction &time,
                       const std::multimap<ReducedFraction, TupletData> &tupletEvents);

std::multimap<ReducedFraction, TupletInfo>
findAllTuplets(std::multimap<ReducedFraction, MidiChord> &chords,
               const TimeSigMap *sigmap,
               const ReducedFraction &lastTick);

void removeEmptyTuplets(MTrack &track);

void addElementToTuplet(int voice,
                        const ReducedFraction &onTime,
                        const ReducedFraction &len,
                        DurationElement *el,
                        std::multimap<ReducedFraction, TupletData> &tuplets);

void createTuplets(Staff *staff,
                   const std::multimap<ReducedFraction, TupletData> &tuplets);

std::multimap<ReducedFraction, TupletData>
convertToData(const std::multimap<ReducedFraction, TupletInfo> &tuplets);

} // namespace MidiTuplet
} // namespace Ms


#endif // IMPORTMIDI_TUPLET_H
