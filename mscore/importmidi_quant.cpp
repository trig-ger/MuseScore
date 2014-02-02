#include "importmidi_quant.h"
#include "libmscore/sig.h"
#include "importmidi_fraction.h"
#include "libmscore/mscore.h"
#include "preferences.h"
#include "importmidi_inner.h"
#include "importmidi_chord.h"
#include "importmidi_meter.h"
#include "importmidi_tuplet.h"
#include "thirdparty/beatroot/BeatTracker.h"

#include <set>
#include <functional>


namespace Ms {

extern Preferences preferences;

namespace Quantize {

ReducedFraction shortestQuantizedNoteInBar(
            const std::multimap<ReducedFraction, MidiChord>::const_iterator &startBarChordIt,
            const std::multimap<ReducedFraction, MidiChord>::const_iterator &endChordIt,
            const ReducedFraction &endBarTick)
      {
      const auto division = ReducedFraction::fromTicks(MScore::division);
      auto minDuration = division;
                  // find shortest note in measure
      for (auto it = startBarChordIt; it != endChordIt; ++it) {
            if (it->first >= endBarTick)
                  break;
            for (const auto &note: it->second.notes) {
                  if (note.len < minDuration)
                        minDuration = note.len;
                  }
            }
                  // determine suitable quantization value based on shortest note in measure
      const auto minAllowedDuration = MChord::minAllowedDuration();
      auto shortest = division;
      for ( ; shortest > minAllowedDuration; shortest /= 2) {
            if (shortest <= minDuration)
                  break;
            }
      return shortest;
      }

ReducedFraction userQuantNoteToFraction(MidiOperation::QuantValue quantNote)
      {
      const auto division = ReducedFraction::fromTicks(MScore::division);
      auto userQuantValue = ReducedFraction::fromTicks(preferences.shortestNote);
                  // specified quantization value
      switch (quantNote) {
            case MidiOperation::QuantValue::N_4:
                  userQuantValue = division;
                  break;
            case MidiOperation::QuantValue::N_8:
                  userQuantValue = division / 2;
                  break;
            case MidiOperation::QuantValue::N_16:
                  userQuantValue = division / 4;
                  break;
            case MidiOperation::QuantValue::N_32:
                  userQuantValue = division / 8;
                  break;
            case MidiOperation::QuantValue::N_64:
                  userQuantValue = division / 16;
                  break;
            case MidiOperation::QuantValue::N_128:
                  userQuantValue = division / 32;
                  break;
            case MidiOperation::QuantValue::FROM_PREFERENCES:
            default:
                  break;
            }

      return userQuantValue;
      }

ReducedFraction fixedQuantRaster()
      {
      const auto operations = preferences.midiImportOperations.currentTrackOperations();
      return userQuantNoteToFraction(operations.quantize.value);
      }

ReducedFraction findRegularQuantRaster(
            const std::multimap<ReducedFraction, MidiChord>::const_iterator &startBarChordIt,
            const std::multimap<ReducedFraction, MidiChord>::const_iterator &endChordIt,
            const ReducedFraction &endBarTick)
      {
      const auto operations = preferences.midiImportOperations.currentTrackOperations();
      auto raster = userQuantNoteToFraction(operations.quantize.value);
                  // if user value larger than the smallest note in bar
                  // then use the smallest note to keep faster events
      if (operations.quantize.reduceToShorterNotesInBar) {
            const auto shortest = shortestQuantizedNoteInBar(startBarChordIt, endChordIt,
                                                             endBarTick);
            if (shortest < raster)
                  raster = shortest;
            }
      return raster;
      }

ReducedFraction reduceRasterIfDottedNote(const ReducedFraction &noteLen,
                                         const ReducedFraction &raster)
      {
      auto newRaster = raster;
      const auto div = noteLen / raster;
      const double ratio = div.numerator() * 1.0 / div.denominator();
      if (ratio > 1.4 && ratio < 1.6)       // 1.5: dotted note that is larger than quantization value
            newRaster /= 2;                 // reduce quantization error for dotted notes
      return newRaster;
      }

ReducedFraction quantizeValue(const ReducedFraction &value,
                              const ReducedFraction &raster)
      {
      int valNum = value.numerator() * raster.denominator();
      const int rastNum = raster.numerator() * value.denominator();
      const int commonDen = value.denominator() * raster.denominator();
      valNum = ((valNum + rastNum / 2) / rastNum) * rastNum;
      return ReducedFraction(valNum, commonDen).reduced();
      }

ReducedFraction findQuantRaster(
            const ReducedFraction &time,
            int voice,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tupletEvents,
            const std::multimap<ReducedFraction, MidiChord> &chords,
            const TimeSigMap *sigmap)
      {
      ReducedFraction raster;
      const auto tupletIt = MidiTuplet::findTupletContainsTime(voice, time, tupletEvents);

      if (tupletIt != tupletEvents.end() && time > tupletIt->first)
            raster = tupletIt->second.tupletQuant;   // quantize onTime with tuplet quant
      else {
                        // quantize onTime with regular quant
            int bar, beat, tick;
            sigmap->tickValues(time.ticks(), &bar, &beat, &tick);
            const auto startBarTick = ReducedFraction::fromTicks(sigmap->bar2tick(bar, 0));
            const auto endBarTick = startBarTick
                        + ReducedFraction(sigmap->timesig(startBarTick.ticks()).timesig());
            const auto startBarChordIt = MChord::findFirstChordInRange(
                            startBarTick, endBarTick, chords.begin(), chords.end());
            raster = findRegularQuantRaster(startBarChordIt, chords.end(), endBarTick);
            }
      return raster;
      }

// input chords - sorted by onTime value, onTime values are not repeated

void quantizeChords(std::multimap<ReducedFraction, MidiChord> &chords,
                    const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tupletEvents,
                    const TimeSigMap *sigmap)
      {
      std::multimap<ReducedFraction, MidiChord> quantizedChords;
      for (auto &chordEvent: chords) {
            MidiChord chord = chordEvent.second;     // copy chord
            auto onTime = chordEvent.first;
            auto raster = findQuantRaster(onTime, chord.voice, tupletEvents, chords, sigmap);
            onTime = quantizeValue(onTime, raster);

            for (auto it = chord.notes.begin(); it != chord.notes.end(); ) {
                  auto &note = *it;
                  auto offTime = chordEvent.first + note.len;
                  raster = findQuantRaster(offTime, chord.voice, tupletEvents, chords, sigmap);

                  if (Meter::isSimpleNoteDuration(raster))    // offTime is not inside tuplet
                        raster = reduceRasterIfDottedNote(note.len, raster);
                  offTime = quantizeValue(offTime, raster);
                  note.len = offTime - onTime;

                  if (note.len < MChord::minAllowedDuration()) {
                        it = chord.notes.erase(it);
                        qDebug() << "quantizeChords: note was removed due to its short length";
                        continue;
                        }
                  ++it;
                  }
            if (!chord.notes.isEmpty())
                  quantizedChords.insert({onTime, chord});
            }

      std::swap(chords, quantizedChords);
      }

double findChordSalience1(const QList<MidiNote> &notes, double ticksPerSec)
      {
      ReducedFraction duration(0, 1);
      int pitch = std::numeric_limits<int>::max();
      int velocity = 0;

      for (const MidiNote &note: notes) {
            if (note.len > duration)
                  duration = note.len;
            if (note.pitch < pitch)
                  pitch = note.pitch;
            velocity += note.velo;
            }
      const double durationInSeconds = duration.ticks() / ticksPerSec;

      const double c4 = 84;
      const int pmin = 48;
      const int pmax = 72;

      if (pitch < pmin)
            pitch = pmin;
      else if (pitch > pmax)
            pitch = pmax;

      if (velocity <= 0)
            velocity = 1;

      return durationInSeconds * (c4 - pitch) * std::log(velocity);
      }

double findChordSalience2(const QList<MidiNote> &notes, double)
      {
      int velocity = 0;
      for (const MidiNote &note: notes) {
            velocity += note.velo;
            }
      if (velocity <= 0)
            velocity = 1;

      return velocity;
      }

std::multimap<int, MTrack>
getTrackWithAllChords(const std::multimap<int, MTrack> &tracks)
      {
      std::multimap<int, MTrack> singleTrack{{0, MTrack()}};
      auto &allChords = singleTrack.begin()->second.chords;
      for (const auto &track: tracks) {
            const MTrack &t = track.second;
            for (const auto &chord: t.chords) {
                  allChords.insert(chord);
                  }
            }
      return singleTrack;
      }

bool isHumanPerformance(const std::multimap<ReducedFraction, MidiChord> &chords)
      {
      if (chords.empty())
            return false;
      auto raster = ReducedFraction::fromTicks(MScore::division) / 4;    // 1/16
      int matches = 0;
      for (const auto &chord: chords) {
            const auto diff = (quantizeValue(chord.first, raster) - chord.first).absValue();
            if (diff < MChord::minAllowedDuration())
                  ++matches;
            }
                  // Min beat-divisions match fraction for machine-generated MIDI.
                  //   During some tests largest human value was 0.315966 with 0.26 on average,
                  //   smallest machine value was 0.423301 with 0.78 on average
      const double tolFraction = 0.4;

      return matches * 1.0 / chords.size() < tolFraction;
      }

::EventList prepareChordEvents(
            const std::multimap<ReducedFraction, MidiChord> &chords,
            const std::function<double(const QList<MidiNote> &, double)> &findChordSalience,
            double ticksPerSec)
      {
      ::EventList events;
      double minSalience = std::numeric_limits<double>::max();
      for (const auto &chord: chords) {
            ::Event e;
            e.time = chord.first.ticks() / ticksPerSec;
            e.salience = findChordSalience(chord.second.notes, ticksPerSec);
            if (e.salience < minSalience)
                  minSalience = e.salience;
            events.push_back(e);
            }
                  // all saliences should be non-negative
      if (minSalience < 0) {
            for (auto &e: events) {
                  e.salience -= minSalience;
                  }
            }

      return events;
      }

std::set<ReducedFraction>
prepareHumanBeatSet(const std::vector<double> &beatTimes,
                    const std::multimap<ReducedFraction, MidiChord> &chords,
                    double ticksPerSec,
                    size_t beatsInBar)
      {
      std::set<ReducedFraction> beatSet;
      for (const auto &beatTime: beatTimes)
            beatSet.insert(MidiTempo::time2Tick(beatTime, ticksPerSec));
                  // first beat time can be larger than first chord onTime
                  // so insert additional beats at the beginning to cover all chords
      const auto &firstOnTime = chords.begin()->first;
      auto firstBeat = *beatSet.begin();
      if (firstOnTime < firstBeat) {
            if (beatSet.size() > 1) {
                  const auto beatLen = *std::next(beatSet.begin()) - firstBeat;
                  size_t counter = 0;
                  do {
                        firstBeat -= beatLen;
                        beatSet.insert(firstBeat);
                        ++counter;
                        } while (firstBeat > firstOnTime || counter % beatsInBar);
                  }
            }

      return beatSet;
      }

double findMatchRank(const std::set<ReducedFraction> &beatSet,
                     const ::EventList &events,
                     const std::vector<int> &levels,
                     size_t beatsInBar,
                     double ticksPerSec)
{
      std::map<ReducedFraction, double> saliences;
      for (const auto &e: events) {
            saliences.insert({MidiTempo::time2Tick(e.time, ticksPerSec), e.salience});
            }
      std::vector<ReducedFraction> beatsOfBar;
      double matchFrac = 0;
      size_t matchCount = 0;
      size_t beatCount = 0;

      for (const auto &beat: beatSet) {
            beatsOfBar.push_back(beat);
            ++beatCount;
            if (beatCount == beatsInBar) {
                  beatCount = 0;
                  size_t relationCount = 0;
                  size_t relationMatches = 0;
                  for (size_t i = 0; i != beatsOfBar.size() - 1; ++i) {
                        const auto s1 = saliences.find(beatsOfBar[i]);
                        for (size_t j = i + 1; j != beatsOfBar.size(); ++j) {
                              ++relationCount;    // before s1 search check
                              if (s1 == saliences.end())
                                    continue;
                              const auto s2 = saliences.find(beatsOfBar[j]);
                              if (s2 == saliences.end())
                                    continue;
                              if ((s1->second < s2->second) == (levels[i] < levels[j]))
                                    ++relationMatches;
                              }
                        }
                  if (relationCount) {
                        matchFrac += relationMatches * 1.0 / relationCount;
                        ++matchCount;
                        }
                  beatsOfBar.clear();
                  }
            }
      if (matchCount)
            matchFrac /= matchCount;

      return matchFrac;
}

void checkForHumanPerformance(const std::multimap<int, MTrack> &tracks,
                              const TimeSigMap *sigmap,
                              double ticksPerSec)
      {
      auto allChordsTrack = getTrackWithAllChords(tracks);
      MChord::collectChords(allChordsTrack, ticksPerSec);
      const MTrack &track = allChordsTrack.begin()->second;
      const auto &allChords = track.chords;
      if (allChords.empty())
            return;

      if (isHumanPerformance(allChords)) {
            const auto barFraction = ReducedFraction(sigmap->timesig(0).timesig());
            const auto beatLen = Meter::beatLength(barFraction);
            const auto div = barFraction / beatLen;
            const size_t beatsInBar = div.numerator() / div.denominator();
            std::vector<Meter::DivisionInfo> divsInfo = {
                        Meter::metricDivisionsOfBar(barFraction) };
            const std::vector<int> levels = Meter::metricLevelsOfBar(barFraction, divsInfo, beatLen);

            Q_ASSERT_X(levels.size() == beatsInBar,
                       "Quantize::checkForHumanPerformance", "Wrong count of levels of bar");

            const std::vector<std::function<double(const QList<MidiNote> &, double)>> salienceFuncs
                        = {findChordSalience1, findChordSalience2};
            std::map<double, std::set<ReducedFraction>, std::greater<double>> beatResults;

            for (const auto &func: salienceFuncs) {
                  const auto events = prepareChordEvents(allChords, func, ticksPerSec);
                  const auto beatTimes = BeatTracker::beatTrack(events);

                  if (!beatTimes.empty()) {
                        auto beatSet = prepareHumanBeatSet(beatTimes, allChords,
                                                           ticksPerSec, beatsInBar);
                        double matchRank = findMatchRank(beatSet, events,
                                                         levels, beatsInBar, ticksPerSec);
                        beatResults.insert({matchRank, std::move(beatSet)});
                        }
                  }
            if (!beatResults.empty()) {
                  preferences.midiImportOperations.setHumanBeats(beatResults.begin()->second);
                  }
            }
      }

void adjustChordsToBeats(std::multimap<int, MTrack> &tracks,
                         ReducedFraction &lastTick)
      {
      const auto &opers = preferences.midiImportOperations;
      const auto &beats = opers.getHumanBeats();
      if (!beats->empty() && opers.trackOperations(0).quantize.humanPerformance) {
            for (auto trackIt = tracks.begin(); trackIt != tracks.end(); ++trackIt) {
                  auto &chords = trackIt->second.chords;
                  if (chords.empty())
                        continue;
                             // do chord alignment according to recognized beats
                  std::multimap<ReducedFraction, MidiChord> newChords;
                  lastTick = {0, 1};

                  auto chordIt = chords.begin();
                  auto it = beats->begin();
                  auto beatStart = *it;
                  auto correctedBeatStart = ReducedFraction(0, 1);

                  for (; it != beats->end(); ++it) {
                        const auto &beatEnd = *it;
                        if (beatEnd == beatStart)
                              continue;
                        const auto desiredBeatLen = ReducedFraction::fromTicks(MScore::division);
                        const auto scale = desiredBeatLen / (beatEnd - beatStart);

                        for (; chordIt != chords.end() && chordIt->first < beatEnd; ++chordIt) {
                                          // quantize to prevent ReducedFraction overflow
                              const auto tickInBar = quantizeValue(
                                    (chordIt->first - beatStart) * scale, MChord::minAllowedDuration());
                              MidiChord ch = chordIt->second;
                              const auto newChordTick = correctedBeatStart + tickInBar;
                              for (auto &note: ch.notes) {
                                    note.len = quantizeValue(note.len * scale,
                                                             MChord::minAllowedDuration());
                                    if (newChordTick + note.len > lastTick)
                                          lastTick = newChordTick + note.len;
                                    }
                              newChords.insert({newChordTick, chordIt->second});
                              }
                        if (chordIt == chords.end())
                              break;

                        beatStart = beatEnd;
                        correctedBeatStart += desiredBeatLen;
                        }

                  std::swap(chords, newChords);
                  }
            }
      }

void simplifyForNotation(std::multimap<int, MTrack> &tracks, const TimeSigMap *sigmap)
      {
      const auto &opers = preferences.midiImportOperations;
      const auto duration8th = ReducedFraction::fromTicks(MScore::division / 2);

      for (auto &trackItem: tracks) {
            MTrack &track = trackItem.second;
            if (!opers.trackOperations(track.indexOfOperation).quantize.humanPerformance)
                  continue;
            bool changed = false;
                        // all chords here with the same voice should have different onTime values
            for (auto it = track.chords.begin(); it != track.chords.end(); ++it) {
                  const auto newLen = MChord::findOptimalNoteLen(it, track.chords, sigmap);

                  auto *maxNote = &*(it->second.notes.begin());
                  for (auto &note: it->second.notes) {
                        if (note.len > maxNote->len)
                              maxNote = &note;
                        }

                  if (maxNote->len < newLen) {     // only enlarge notes
                        const auto gap = newLen - maxNote->len;
                        if (newLen >= duration8th) {
                              if (gap >= newLen / 2) {

                                    int bar, beat, tickInBar;
                                    sigmap->tickValues(it->first.ticks(), &bar, &beat, &tickInBar);
                                    const auto startBarTick = ReducedFraction::fromTicks(
                                                      sigmap->bar2tick(bar, 0));
                                    const auto barFraction = ReducedFraction(
                                                      sigmap->timesig(startBarTick.ticks()).timesig());

                                    auto tupletsInBar = MidiTuplet::findTupletsInBarForDuration(
                                                      it->second.voice, startBarTick, it->first,
                                                      newLen, track.tuplets);
                                    struct {
                                          bool operator()(const MidiTuplet::TupletData &d1,
                                                          const MidiTuplet::TupletData &d2)
                                                {
                                                return (d1.len > d2.len);
                                                }
                                          } comparator;
                                                // sort by tuplet length in desc order
                                    sort(tupletsInBar.begin(), tupletsInBar.end(), comparator);

                                    const auto divInfo = Meter::divisionInfo(barFraction, tupletsInBar);
                                    const int begLevel = Meter::levelOfTick(it->first, divInfo);
                                    const int endLevel = Meter::levelOfTick(it->first + newLen, divInfo);
                                    const auto splitPoint = Meter::findMaxLevelBetween(
                                                      it->first, it->first + newLen, divInfo);
                                    if ((splitPoint.level > begLevel || splitPoint.level > endLevel)
                                                && Meter::isSingleDottedDuration(newLen)) {
                                          maxNote->len = newLen;
                                          }
                                    }
                              else {
                                    maxNote->len = newLen;
                                    if (gap >= newLen / 4 && gap < newLen / 2)
                                          it->second.staccato = true;
                                    }
                              }
                        else {
                              maxNote->len = newLen;
                              if (gap < newLen / 2)
                                    it->second.staccato = true;
                              }

                        if (!changed)
                              changed = true;
                        }
                  }
            if (changed)
                  MidiTuplet::removeEmptyTuplets(track);
            }
      }

} // namespace Quantize
} // namespace Ms
