#include "importmidi_simplify.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_tuplet.h"
#include "importmidi_quant.h"
#include "preferences.h"
#include "libmscore/sig.h"
#include "libmscore/durationtype.h"
#include "importmidi_tuplet_voice.h"


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

bool areNotesSortedByPitchInAscOrder(const QList<MidiNote>& notes)
      {
      for (int i = 0; i != notes.size() - 1; ++i) {
            if (notes[i].pitch > notes[i + 1].pitch)
                  return false;
            }
      return true;
      }

bool areNotesSortedByOffTimeInAscOrder(const QList<MidiNote>& notes)
      {
      for (int i = 0; i != notes.size() - 1; ++i) {
            if (notes[i].offTime > notes[i + 1].offTime)
                  return false;
            }
      return true;
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

bool allNotesHaveEqualLength(const QList<MidiNote> &notes)
      {
      const auto &offTime = notes[0].offTime;
      for (int i = 1; i != notes.size(); ++i) {
            if (notes[i].offTime != offTime)
                  return false;
            }
      return true;
      }

int findDurationCountInGroup(
            const ReducedFraction &chordOnTime,
            const QList<MidiNote> &notes,
            int voice,
            const std::vector<int> &groupOfIndexes,
            const TimeSigMap *sigmap,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      Q_ASSERT_X(areNotesSortedByOffTimeInAscOrder(notes),
                 "Simplify::findDurationCountInGroup",
                 "Notes are not sorted by off time in ascending order");

      const auto &opers = preferences.midiImportOperations.currentTrackOperations();
      const bool useDots = opers.useDots;

      int count = 0;
      auto onTime = chordOnTime;
      auto onTimeBarStart = findBarStart(onTime, sigmap);
      auto onTimeBarFraction = ReducedFraction(
                        sigmap->timesig(onTimeBarStart.ticks()).timesig());

      for (int i: groupOfIndexes) {
            const auto &offTime = notes[i].offTime;
            if (offTime == onTime)
                  continue;
            const auto offTimeBarStart = findBarStart(offTime, sigmap);

            if (offTimeBarStart != onTimeBarStart) {
                  const auto offTimeBarFraction = ReducedFraction(
                              sigmap->timesig(offTimeBarStart.ticks()).timesig());

                  const auto tupletsForDuration = MidiTuplet::findTupletsInBarForDuration(
                              voice, onTimeBarStart, onTime, offTimeBarStart - onTime, tuplets);

                              // additional durations on measure boundary
                  const auto durations = Meter::toDurationList(
                              onTime - onTimeBarStart, offTimeBarStart - onTimeBarStart,
                              offTimeBarFraction, tupletsForDuration, Meter::DurationType::NOTE,
                              useDots, false);

                  count += durationCount(durations);

                  onTime = offTimeBarStart;
                  onTimeBarStart = offTimeBarStart;
                  onTimeBarFraction = offTimeBarFraction;
                  }

            const auto tupletsForDuration = MidiTuplet::findTupletsInBarForDuration(
                              voice, onTimeBarStart, onTime, offTime - onTime, tuplets);

            const auto durations = Meter::toDurationList(
                              onTime - onTimeBarStart, offTime - onTimeBarStart, onTimeBarFraction,
                              tupletsForDuration, Meter::DurationType::NOTE, useDots, false);

            count += durationCount(durations);

            onTime = offTime;
            }
      return count;
      }

// count of resulting durations in music notation

int findDurationCount(
            const QList<MidiNote> &notes,
            int voice,
            int splitPoint,
            const ReducedFraction &chordOnTime,
            const TimeSigMap *sigmap,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      std::vector<int> lowGroup;
      std::vector<int> highGroup;

      for (int i = 0; i != splitPoint; ++i)
            lowGroup.push_back(i);
      for (int i = splitPoint + 1; i != notes.size(); ++i)
            highGroup.push_back(i);

      std::sort(lowGroup.begin(), lowGroup.end(),
                [&](int i1, int i2) { return notes[i1].offTime < notes[i2].offTime; });
      std::sort(highGroup.begin(), highGroup.end(),
                [&](int i1, int i2) { return notes[i1].offTime < notes[i2].offTime; });

      return findDurationCountInGroup(chordOnTime, notes, voice, lowGroup, sigmap, tuplets)
             + findDurationCountInGroup(chordOnTime, notes, voice, highGroup, sigmap, tuplets);
      }

int findOptimalSplitPoint(
            const QList<MidiNote>& notes,
            int voice,
            const ReducedFraction &chordOnTime,
            const TimeSigMap *sigmap,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      Q_ASSERT_X(!notes.isEmpty(),
                 "Simplify::findOptimalSplitPoint", "Notes are empty");
      Q_ASSERT_X(areNotesSortedByPitchInAscOrder(notes),
                 "Simplify::findOptimalSplitPoint",
                 "Notes are not sorted by pitch in ascending order");

      if (allNotesHaveEqualLength(notes))
            return -1;

      int minNoteCount = std::numeric_limits<int>::max();
      int minSplit;

      for (int splitPoint = 0; splitPoint != notes.size(); ++splitPoint) {
            int noteCount = findDurationCount(notes, voice, splitPoint,
                                              chordOnTime, sigmap, tuplets);
            if (noteCount < minNoteCount) {
                  minNoteCount = noteCount;
                  minSplit = splitPoint;
                  }
            }

      return minSplit;
      }

// which part of chord - low notes or high notes - should be shifted to another voice

enum class ShiftedPitchGroup {
      LOW,
      HIGH
      };

struct VoiceSplit {
      ShiftedPitchGroup group;
      int voice = -1;
      int penalty = -1;
      };

struct Step {
      std::vector<VoiceSplit> possibleVoiceSplits;
      ReducedFraction onTime;
      MidiChord *chord = nullptr;
            // splitPoint - note index, such that: LOW part = [0, splitPoint]
            // and HIGH part = (splitPoint, size)
      int splitPoint = -1;
      int prevSplit = -1;     // index in possibleVoiceSplits
      };

int findLastStepSplit(const std::vector<Step> &steps)
      {
      int splitIndex = -1;
      int minPenalty = std::numeric_limits<int>::max();
      const auto &lastSplits = steps[steps.size() - 1].possibleVoiceSplits;
      for (int i = 0; i != (int)lastSplits.size(); ++i) {
            if (lastSplits[i].penalty < minPenalty) {
                  minPenalty = lastSplits[i].penalty;
                  splitIndex = i;
                  }
            }

      Q_ASSERT_X(splitIndex != -1,
                 "Simplify::findLastStepSplit", "Last split index was not found");

      return splitIndex;
      }

void applySplit(
            const Step &step,
            const VoiceSplit &split,
            std::multimap<ReducedFraction, MidiChord> &chords)
      {
      MidiChord &oldChord = *step.chord;
      MidiChord newChord(oldChord);
      int beg = 0;
      int end = oldChord.notes.size();

      switch (split.group) {
            case ShiftedPitchGroup::LOW:
                  end = step.splitPoint;
                  break;
            case ShiftedPitchGroup::HIGH:
                  beg = step.splitPoint + 1;
                  break;
            }
      newChord.notes.clear();
      for (int i = beg; i != end; ++i)
            newChord.notes.append(oldChord.notes[i]);
      for (int i = beg; i != end; ++i)
            oldChord.notes.removeAt(i);
      newChord.voice = split.voice;

      chords.insert({step.onTime, newChord});
      }

// backward dynamic programming step - collect optimal voice separations

void collectSolution(
            const std::vector<Step> &steps,
            std::multimap<ReducedFraction, MidiChord> &chords)
      {
      int splitIndex = findLastStepSplit(steps);

      for (int stepIndex = steps.size() - 1; ; --stepIndex) {
            const Step &step = steps[stepIndex];
            const VoiceSplit &split = step.possibleVoiceSplits[splitIndex];
            applySplit(step, split, chords);

            if (stepIndex == 0)
                  break;
            splitIndex = step.prevSplit;
            }
      }

// it's an optimization function: we can don't check chords
// with (on time + max chord len) < given time moment
// because chord cannot be longer than found max length
// result: <voice, max chord length>

std::map<int, ReducedFraction> findMaxChordLengths(
            const std::multimap<ReducedFraction, MidiChord> &chords)
      {
      std::map<int, ReducedFraction> maxLengths;

      for (const auto &chord: chords) {
            const auto offTime = MChord::maxNoteOffTime(chord.second.notes);
            if (offTime - chord.first > maxLengths[chord.second.voice])
                  maxLengths[chord.second.voice] = offTime - chord.first;
            }
      return maxLengths;
      }

bool hasIntersectionWithTuplets(
            int voice,
            const ReducedFraction &onTime,
            const ReducedFraction &offTime,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      auto it = MidiTuplet::findTupletForTimeRange(voice, onTime, offTime - onTime, tuplets);
      return it != tuplets.end();
      }

bool hasIntersectionWithChords(
            int voice,
            std::map<int, ReducedFraction> &maxChordLengths,
            const ReducedFraction &onTime,
            const ReducedFraction &offTime,
            const std::multimap<ReducedFraction, MidiChord> &chords)
      {
      auto it = chords.lower_bound(offTime);
      if (it == chords.begin())
            return true;
      bool hasIntersection = false;
      while (it->first + maxChordLengths[voice] > onTime) {
            if (MidiTuplet::haveIntersection({it->first, MChord::maxNoteOffTime(it->second.notes)},
                                             {onTime, offTime})) {
                  hasIntersection = true;
                  break;
                  }
            if (it == chords.begin())
                  break;
            --it;
            }

      return hasIntersection;
      }

void addGroupSplits(
            std::vector<VoiceSplit> &splits,
            std::map<int, ReducedFraction> &maxChordLengths,
            const std::multimap<ReducedFraction, MidiChord> &chords,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
            const ReducedFraction &onTime,
            const ReducedFraction &groupOffTime,
            int origVoice,
            ShiftedPitchGroup groupType)
      {
      const int voiceLimit = MidiTuplet::voiceLimit();

      for (int voice = 0; voice != voiceLimit; ++voice) {
            if (voice == origVoice)
                  continue;
            if (hasIntersectionWithTuplets(voice, onTime, groupOffTime, tuplets))
                  continue;
            if (hasIntersectionWithChords(voice, maxChordLengths,
                                          onTime, groupOffTime, chords)) {
                  continue;
                  }

            VoiceSplit split;
            split.group = groupType;
            split.voice = voice;
            splits.push_back(split);
            }
      }

std::vector<VoiceSplit> findPossibleVoiceSplits(
            int origVoice,
            const QList<MidiNote> &notes,
            int splitPoint,
            const std::multimap<ReducedFraction, MidiChord> &chords,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      std::vector<VoiceSplit> splits;

      ReducedFraction onTime;
      ReducedFraction lowGroupOffTime;
      ReducedFraction highGroupOffTime;

      for (int i = 0; i != splitPoint; ++i) {
            if (notes[i].offTime > lowGroupOffTime)
                  lowGroupOffTime = notes[i].offTime;
            }
      for (int i = splitPoint + 1; i != notes.size(); ++i) {
            if (notes[i].offTime > highGroupOffTime)
                  highGroupOffTime = notes[i].offTime;
            }

      std::map<int, ReducedFraction> maxChordLengths = findMaxChordLengths(chords);

      addGroupSplits(splits, maxChordLengths, chords, tuplets, onTime, lowGroupOffTime,
                     origVoice, ShiftedPitchGroup::LOW);
      addGroupSplits(splits, maxChordLengths, chords, tuplets, onTime, highGroupOffTime,
                     origVoice, ShiftedPitchGroup::HIGH);

      return splits;
      }

void calculatePenalties(std::vector<Step> &steps)
      {

      }

std::vector<Step> findSteps(
            std::multimap<ReducedFraction, MidiChord> &chords,
            const TimeSigMap *sigmap,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      std::vector<Step> steps;

      for (auto &chord: chords) {
            auto &notes = chord.second.notes;
            const int splitPoint = findOptimalSplitPoint(
                              notes, chord.second.voice, chord.first, sigmap, tuplets);
            if (splitPoint == -1)
                  continue;
            Step step;
            step.onTime = chord.first;
            step.chord = &chord.second;
            step.possibleVoiceSplits = findPossibleVoiceSplits(
                              chord.second.voice, notes, splitPoint, chords, tuplets);
            step.splitPoint = splitPoint;
            steps.push_back(step);
            }

      return steps;
      }

void separateVoices(
            std::multimap<ReducedFraction, MidiChord> &chords,
            const TimeSigMap *sigmap,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets)
      {
      MChord::sortNotesByPitch(chords);
      std::vector<Step> steps = findSteps(chords, sigmap, tuplets);
      calculatePenalties(steps);
      collectSolution(steps, chords);
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

            if (MidiTuplet::voiceLimit() > 1)
                  separateVoices(chords, sigmap, mtrack.tuplets);

            if (opers.currentTrackOperations().minimizeNumberOfRests)
                  minimizeNumberOfRests(chords, sigmap, mtrack.tuplets);
            }
      }

} // Simplify
} // Ms
