#include "importmidi_simplify.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_tuplet.h"
#include "importmidi_quant.h"
#include "preferences.h"
#include "libmscore/sig.h"
#include "libmscore/durationtype.h"


namespace Ms {
namespace Simplify {

const ReducedFraction findBarStart(const ReducedFraction &time, const TimeSigMap *sigmap)
      {
      int barIndex, beat, tick;
      sigmap->tickValues(time.ticks(), &barIndex, &beat, &tick);
      return ReducedFraction::fromTicks(sigmap->bar2tick(barIndex, 0));
      }

double durationCount(const QList<std::pair<ReducedFraction, TDuration> > &durations)
      {
      double count = durations.size();
      for (const auto &d: durations) {
            if (d.second.dots())
                  count += 0.5;
            }
      return count;
      }

bool hasComplexBeamedDurations(const QList<std::pair<ReducedFraction, TDuration> > &list)
      {
      for (const auto &d: list) {
            if (d.second == TDuration::V_16TH || d.second == TDuration::V_32ND
                        || d.second == TDuration::V_64TH || d.second == TDuration::V_128TH) {
                  return true;
                  }
            }
      return false;
      }


#ifdef QT_DEBUG

bool areDurationsEqual(
            const QList<std::pair<ReducedFraction, TDuration> > &durations,
            const ReducedFraction &desiredLen)
      {
      ReducedFraction sum(0, 1);
      for (const auto &d: durations)
            sum += ReducedFraction(d.second.fraction()) / d.first;

      return desiredLen == desiredLen;
      }

#endif


void lengthenNote(
            MidiNote &note,
            int voice,
            const ReducedFraction &noteOnTime,
            const ReducedFraction &durationStart,
            const ReducedFraction &endTime,
            const ReducedFraction &barStart,
            const ReducedFraction &barFraction,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      if (endTime <= note.offTime)
            return;

      const auto &opers = preferences.midiImportOperations.currentTrackOperations();
      const bool useDots = opers.useDots;
      const auto tupletsForDuration = MidiTuplet::findTupletsInBarForDuration(
                              voice, barStart, note.offTime, endTime - note.offTime, tuplets);

      Q_ASSERT_X(note.quant != ReducedFraction(-1, 1),
                 "Simplify::lengthenNote", "Note quant value was not set");

      const auto origNoteDurations = Meter::toDurationList(
                              durationStart - barStart, note.offTime - barStart, barFraction,
                              tupletsForDuration, Meter::DurationType::NOTE, useDots, false);

      const auto origRestDurations = Meter::toDurationList(
                              note.offTime - barStart, endTime - barStart, barFraction,
                              tupletsForDuration, Meter::DurationType::REST, useDots, false);

      Q_ASSERT_X(areDurationsEqual(origNoteDurations, note.offTime - durationStart),
                 "Simplify::lengthenNote", "Too short note durations remaining");
      Q_ASSERT_X(areDurationsEqual(origRestDurations, endTime - note.offTime),
                 "Simplify::lengthenNote", "Too short rest durations remaining");

                  // double - because can be + 0.5 for dots
      double minNoteDurationCount = durationCount(origNoteDurations);
      double minRestDurationCount = durationCount(origRestDurations);

      ReducedFraction bestOffTime(-1, 1);

      for (ReducedFraction offTime = note.offTime + note.quant;
                           offTime <= endTime; offTime += note.quant) {
            double noteDurationCount = 0;
            double restDurationCount = 0;

            const auto noteDurations = Meter::toDurationList(
                              durationStart - barStart, offTime - barStart, barFraction,
                              tupletsForDuration, Meter::DurationType::NOTE, useDots, false);

            Q_ASSERT_X(areDurationsEqual(noteDurations, offTime - durationStart),
                       "Simplify::lengthenNote", "Too short note durations remaining");

            noteDurationCount += durationCount(noteDurations);

            if (offTime < endTime) {
                  const auto restDurations = Meter::toDurationList(
                              offTime - barStart, endTime - barStart, barFraction,
                              tupletsForDuration, Meter::DurationType::REST, useDots, false);

                  Q_ASSERT_X(areDurationsEqual(restDurations, endTime - offTime),
                             "Simplify::lengthenNote", "Too short rest durations remaining");

                  restDurationCount += durationCount(restDurations);
                  }

            if (noteDurationCount + restDurationCount
                              < minNoteDurationCount + minRestDurationCount) {
                  if (opers.quantize.humanPerformance || noteDurationCount == 1) {
                        minNoteDurationCount = noteDurationCount;
                        minRestDurationCount = restDurationCount;
                        bestOffTime = offTime;
                        }
                  }
            }

      if (bestOffTime == ReducedFraction(-1, 1))
            return;

      // check for staccato:
      //    don't apply staccato if note is tied
      //    (case noteOnTime != durationStart - another bar, for example)

      bool hasLossOfAccuracy = false;
      const double addedPart = ((bestOffTime - note.offTime)
                                / (bestOffTime - durationStart)).toDouble();
      const double STACCATO_TOL = 0.3;

      if (addedPart >= STACCATO_TOL) {
            if (noteOnTime == durationStart && minNoteDurationCount == 1)
                  note.staccato = true;
            else
                  hasLossOfAccuracy = true;
            }

      // if the difference is only in one note/rest and there is some loss of accuracy -
      // discard change because it silently reduces duration accuracy
      // without significant improvement of readability

      if (!opers.quantize.humanPerformance
                  && (origNoteDurations.size() + origRestDurations.size())
                       - (minNoteDurationCount + minRestDurationCount) <= 1
                  && !hasComplexBeamedDurations(origNoteDurations)
                  && !hasComplexBeamedDurations(origRestDurations)
                  && hasLossOfAccuracy) {
            return;
            }

      note.offTime = bestOffTime;
      }

void minimizeNumberOfRests(
            std::multimap<ReducedFraction, MidiChord> &chords,
            const TimeSigMap *sigmap,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      for (auto it = chords.begin(); it != chords.end(); ++it) {
            for (MidiNote &note: it->second.notes) {
                  const auto barStart = findBarStart(note.offTime, sigmap);
                  const auto barFraction = ReducedFraction(
                                                sigmap->timesig(barStart.ticks()).timesig());
                  auto durationStart = (it->first > barStart) ? it->first : barStart;
                  if (it->second.isInTuplet) {
                        const auto &tuplet = it->second.tuplet->second;
                        if (note.offTime >= tuplet.onTime + tuplet.len)
                              durationStart = tuplet.onTime + tuplet.len;
                        }
                  auto endTime = (barStart == note.offTime)
                                    ? barStart : barStart + barFraction;
                  if (note.isInTuplet) {
                        const auto &tuplet = note.tuplet->second;
                        if (note.offTime == tuplet.onTime + tuplet.len)
                              continue;
                        endTime = barStart + Quantize::quantizeToLarge(
                                    note.offTime - barStart, tuplet.len / tuplet.tupletNumber);
                        }

                  const auto beatLen = Meter::beatLength(barFraction);
                  const auto beatTime = barStart + Quantize::quantizeToLarge(
                                                      note.offTime - barStart, beatLen);
                  if (endTime > beatTime)
                        endTime = beatTime;

                  auto next = std::next(it);
                  while (next != chords.end()
                              && (next->second.voice != it->second.voice
                                  || next->first < note.offTime)) {
                        ++next;
                        }
                  if (next != chords.end()) {
                        if (next->first < endTime)
                              endTime = next->first;
                        if (next->second.isInTuplet
                                    && !note.isInTuplet && next->second.tuplet == note.tuplet) {
                              const auto &tuplet = next->second.tuplet->second;
                              if (tuplet.onTime < endTime)
                                    endTime = tuplet.onTime;
                              }
                        }

                  lengthenNote(note, it->second.voice, it->first, durationStart, endTime,
                               barStart, barFraction, tuplets);
                  }
            }
      }

void separateVoices(std::multimap<ReducedFraction, MidiChord> &chords)
      {

      }

void simplifyNotation(std::multimap<int, MTrack> &tracks, const TimeSigMap *sigmap)
      {
      auto &opers = preferences.midiImportOperations;

      for (auto &track: tracks) {
            MTrack &mtrack = track.second;
            if (mtrack.mtrack->drumTrack())
                  continue;
            auto &chords = track.second.chords;
            if (chords.empty())
                  continue;
                        // pass current track index through MidiImportOperations
                        // for further usage
            opers.setCurrentTrack(mtrack.indexOfOperation);

            if (opers.currentTrackOperations().allowedVoices != MidiOperation::AllowedVoices::V_1)
                  separateVoices(chords);

            if (opers.currentTrackOperations().minimizeNumberOfRests)
                  minimizeNumberOfRests(chords, sigmap, mtrack.tuplets);
            }
      }

} // Simplify
} // Ms
