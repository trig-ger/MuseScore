#include "importmidi_lrhand.h"
#include "importmidi_tuplet.h"
#include "importmidi_inner.h"
#include "importmidi_fraction.h"
#include "importmidi_chord.h"
#include "preferences.h"


namespace Ms {

extern Preferences preferences;

namespace LRHand {

bool needToSplit(const std::multimap<ReducedFraction, MidiChord> &chords, int midiProgram)
      {
      if (!MChord::isGrandStaffProgram(midiProgram))
            return false;

      const int octave = 12;
      int chordCount = 0;
      for (const auto &chord: chords) {
            const MidiChord &c = chord.second;
            int minPitch = std::numeric_limits<int>::max();
            int maxPitch = 0;
            for (const auto &note: c.notes) {
                  if (note.pitch < minPitch)
                        minPitch = note.pitch;
                  if (note.pitch > maxPitch)
                        maxPitch = note.pitch;
                  }
            if (maxPitch - minPitch > octave)
                  return true;
            if (c.notes.size() > 1)
                  ++chordCount;
            }

      if (chordCount > (int)chords.size() / 2)
            return true;
      return false;
      }


#ifdef QT_DEBUG

bool areNotesSortedByPitchInAscOrder(const QList<MidiNote>& notes)
      {
      for (int i = 0; i != notes.size() - 1; ++i) {
            if (notes[i].pitch > notes[i + 1].pitch)
                  return false;
            }
      return true;
      }

#endif


struct SplitTry {
      int penalty = 0;
                  // split point - note index, such that: LOW part = [0, split point)
                  // and HIGH part = [split point, size)
                  // if split point = size then the chord is assigned to the left hand
      int prevSplitPoint = -1;
      };

struct ChordSplitData {
      std::multimap<ReducedFraction, MidiChord>::iterator chord;
      std::vector<SplitTry> possibleSplits;    // each index correspoinds to the same note index
      };

int findLastSplitPoint(const std::vector<ChordSplitData> &splits)
      {
      int splitPoint = -1;
      int minPenalty = std::numeric_limits<int>::max();
      const auto &possibleSplits = splits[splits.size() - 1].possibleSplits;

      for (int i = 0; i != (int)possibleSplits.size(); ++i) {
            if (possibleSplits[i].penalty < minPenalty) {
                  minPenalty = possibleSplits[i].penalty;
                  splitPoint = i;
                  }
            }

      Q_ASSERT_X(splitPoint != -1,
                 "LRHand::findLastSplitPoint", "Last split point was not found");

      return splitPoint;
      }

// backward dynamic programming step - collect optimal voice separations

void splitChords(
            const std::vector<ChordSplitData> &splits,
            std::multimap<ReducedFraction, MidiChord> &leftHandChords,
            std::multimap<ReducedFraction, MidiChord> &chords)
      {

      Q_ASSERT_X(splits.size() == chords.size(),
                 "LRHand::collectSolution", "Sizes of split data and chords don't match");

      int splitPoint = findLastSplitPoint(splits);

      for (int pos = splits.size() - 1; ; --pos) {
            MidiChord &oldChord = splits[pos].chord->second;
            MidiChord newChord(oldChord);

            if (splitPoint > 0 && splitPoint < oldChord.notes.size()) {
                  const auto oldNotes = oldChord.notes;

                  oldChord.notes.clear();
                  for (int i = splitPoint; i != oldNotes.size(); ++i)
                        oldChord.notes.append(oldNotes[i]);

                  newChord.notes.clear();
                  for (int i = 0; i != splitPoint; ++i)
                        newChord.notes.append(oldNotes[i]);

                  leftHandChords.insert({splits[pos].chord->first, newChord});
                  }
            else if (splitPoint == oldChord.notes.size()) {
                  chords.erase(splits[pos].chord);
                  leftHandChords.insert({splits[pos].chord->first, newChord});
                  }

            if (pos == 0)
                  break;
            splitPoint = splits[pos].possibleSplits[splitPoint].prevSplitPoint;
            }
      }

int findPitchWidthPenalty(const QList<MidiNote> &notes, int splitPoint)
      {
      const int octave = 12;
      const int maxPitchWidth = octave + 2;
      int penalty = 0;

      if (splitPoint > 0) {
            const int lowPitchWidth = qAbs(notes[0].pitch
                                           - notes[splitPoint - 1].pitch);
            if (lowPitchWidth <= octave)
                  penalty += 0;
            else if (lowPitchWidth <= maxPitchWidth)
                  penalty += 20;
            else
                  penalty += 100;
            }

      if (splitPoint < notes.size()) {
            const int highPitchWidth = qAbs(notes[splitPoint].pitch
                                            - notes[notes.size() - 1].pitch);
            if (highPitchWidth <= octave)
                  penalty += 0;
            else if (highPitchWidth <= maxPitchWidth)
                  penalty += 20;
            else
                  penalty += 100;
            }

      return penalty;
      }

bool isOctave(const QList<MidiNote> &notes, int beg, int end)
      {
      Q_ASSERT_X(end > 0 && beg >= 0 && end > beg, "LRHand::isOctave", "Invalid note indexes");

      const int octave = 12;
      return (end - beg == 2 && notes[end - 1].pitch - notes[beg].pitch == octave);
      }

int findSimilarityPenalty(
            const QList<MidiNote> &notes,
            const QList<MidiNote> &prevNotes,
            int splitPoint,
            int prevSplitPoint)
      {
      int penalty = 0;
                  // check for octaves and accompaniment
      if (splitPoint > 0 && prevSplitPoint > 0) {
            const bool isLowOctave = isOctave(notes, 0, splitPoint);
            const bool isPrevLowOctave = isOctave(prevNotes, 0, prevSplitPoint);

            if (isLowOctave && isPrevLowOctave)             // octaves
                  penalty -= 12;
            else if (splitPoint > 1 && prevSplitPoint > 1)  // accompaniment
                  penalty -= 5;
            }
      if (splitPoint < notes.size() && prevSplitPoint < prevNotes.size()) {
            const bool isHighOctave = isOctave(notes, splitPoint, notes.size());
            const bool isPrevHighOctave = isOctave(prevNotes, prevSplitPoint, prevNotes.size());

            if (isHighOctave && isPrevHighOctave)
                  penalty -= 12;
            else if (notes.size() - splitPoint > 1 && prevNotes.size() - prevSplitPoint > 1)
                  penalty -= 5;
            }
                  // check for one-note melody
      if (splitPoint - 0 == 1 && prevSplitPoint - 0 == 1)
            penalty -= 12;
      if (notes.size() - splitPoint == 1 && prevNotes.size() - prevSplitPoint == 1)
            penalty -= 12;

      return penalty;
      }

bool areOffTimesEqual(const QList<MidiNote> &notes, int beg, int end)
      {
      Q_ASSERT_X(end > 0 && beg >= 0 && end > beg,
                 "LRHand::areOffTimesEqual", "Invalid note indexes");

      bool areEqual = true;
      const ReducedFraction firstOffTime = notes[beg].offTime;
      for (int i = beg + 1; i < end; ++i) {
            if (notes[i].offTime != firstOffTime) {
                  areEqual = false;
                  break;
                  }
            }
      return areEqual;
      }

int findDurationPenalty(const QList<MidiNote> &notes, int splitPoint)
      {
      int penalty = 0;
      if (splitPoint > 0 && areOffTimesEqual(notes, 0, splitPoint))
            penalty -= 10;
      if (splitPoint < notes.size() && areOffTimesEqual(notes, splitPoint, notes.size()))
            penalty -= 10;
      return penalty;
      }

int findNoteCountPenalty(const QList<MidiNote> &notes, int splitPoint)
      {
      const int leftHandCount = splitPoint;
      const int rightHandCount = notes.size() - splitPoint;
      if (rightHandCount > 0 && leftHandCount > 0 && leftHandCount < rightHandCount)
            return 5;
      if (rightHandCount == 0 && leftHandCount > 1)
            return 10;
      return 0;
      }

std::vector<ChordSplitData> findSplits(std::multimap<ReducedFraction, MidiChord> &chords)
      {
      std::vector<ChordSplitData> splits;
      int pos = 0;

      for (auto it = chords.begin(); it != chords.end(); ++it) {
            const auto &notes = it->second.notes;

            Q_ASSERT_X(!notes.isEmpty(),
                       "LRHand::findSplits", "Notes are empty");
            Q_ASSERT_X(areNotesSortedByPitchInAscOrder(notes),
                       "LRHand::findSplits",
                       "Notes are not sorted by pitch in ascending order");

            ChordSplitData split;
            split.chord = it;

            for (int splitPoint = 0; splitPoint <= notes.size(); ++splitPoint) {
                  SplitTry splitTry;
                  splitTry.penalty = findPitchWidthPenalty(notes, splitPoint)
                                    + findDurationPenalty(notes, splitPoint)
                                    + findNoteCountPenalty(notes, splitPoint);

                  if (pos > 0) {
                        int bestPrevSplitPoint = -1;
                        int minPenalty = std::numeric_limits<int>::max();

                        const auto &prevNotes = std::prev(it)->second.notes;
                        for (int prevSplitPoint = 0;
                                 prevSplitPoint <= prevNotes.size(); ++prevSplitPoint) {

                              const int prevPenalty
                                          = splits[pos - 1].possibleSplits[prevSplitPoint].penalty
                                          + findSimilarityPenalty(
                                                    notes, prevNotes, splitPoint, prevSplitPoint);

                              if (prevPenalty < minPenalty) {
                                    minPenalty = prevPenalty;
                                    bestPrevSplitPoint = prevSplitPoint;
                                    }
                              }

                        Q_ASSERT_X(bestPrevSplitPoint != -1,
                                   "LRHand::findSplits",
                                   "Best previous split point was not found");

                        splitTry.penalty += minPenalty;
                        splitTry.prevSplitPoint = bestPrevSplitPoint;
                        }

                  split.possibleSplits.push_back(splitTry);
                  }
            splits.push_back(split);

            ++pos;
            }

      return splits;
      }

void insertNewLeftHandTrack(std::multimap<int, MTrack> &tracks,
                            std::multimap<int, MTrack>::iterator &it,
                            const std::multimap<ReducedFraction, MidiChord> &leftHandChords)
      {
      auto leftHandTrack = it->second;
      leftHandTrack.chords = leftHandChords;
      MidiTuplet::removeEmptyTuplets(leftHandTrack);
      it = tracks.insert({it->first, leftHandTrack});
      }

// maybe todo later: if range of right-hand chords > OCTAVE
// => assign all bottom right-hand chords to another, third track

void splitByHandWidth(std::multimap<int, MTrack> &tracks,
                      std::multimap<int, MTrack>::iterator &it)
      {
      auto &chords = it->second.chords;
      MChord::sortNotesByPitch(chords);
      std::vector<ChordSplitData> splits = findSplits(chords);

      std::multimap<ReducedFraction, MidiChord> leftHandChords;
      splitChords(splits, leftHandChords, chords);

      if (!leftHandChords.empty())
            insertNewLeftHandTrack(tracks, it, leftHandChords);
      }

void addNewLeftHandChord(std::multimap<ReducedFraction, MidiChord> &leftHandChords,
                         const QList<MidiNote> &leftHandNotes,
                         const std::multimap<ReducedFraction, MidiChord>::iterator &it)
      {
      MidiChord leftHandChord = it->second;
      leftHandChord.notes = leftHandNotes;
      leftHandChords.insert({it->first, leftHandChord});
      }

void splitByFixedPitch(std::multimap<int, MTrack> &tracks,
                       std::multimap<int, MTrack>::iterator &it)
      {
      auto &srcTrack = it->second;
      const auto trackOpers = preferences.midiImportOperations.trackOperations(srcTrack.indexOfOperation);
      const int splitPitch = 12 * (int)trackOpers.LHRH.splitPitchOctave
                           + (int)trackOpers.LHRH.splitPitchNote;
      std::multimap<ReducedFraction, MidiChord> leftHandChords;

      for (auto i = srcTrack.chords.begin(); i != srcTrack.chords.end(); ) {
            auto &notes = i->second.notes;
            QList<MidiNote> leftHandNotes;
            for (auto j = notes.begin(); j != notes.end(); ) {
                  auto &note = *j;
                  if (note.pitch < splitPitch) {
                        leftHandNotes.push_back(note);
                        j = notes.erase(j);
                        continue;
                        }
                  ++j;
                  }
            if (!leftHandNotes.empty())
                  addNewLeftHandChord(leftHandChords, leftHandNotes, i);
            if (notes.isEmpty()) {
                  i = srcTrack.chords.erase(i);
                  continue;
                  }
            ++i;
            }
      if (!leftHandChords.empty())
            insertNewLeftHandTrack(tracks, it, leftHandChords);
      }

void splitIntoLeftRightHands(std::multimap<int, MTrack> &tracks)
      {
      for (auto it = tracks.begin(); it != tracks.end(); ++it) {
            if (it->second.mtrack->drumTrack())
                  continue;
            const auto operations = preferences.midiImportOperations.trackOperations(
                                                              it->second.indexOfOperation);
            if (!operations.LHRH.doIt)
                  continue;
                        // iterator 'it' will change after track split to ++it
                        // C++11 guarantees that newely inserted item with equal key will go after:
                        //    "The relative ordering of elements with equivalent keys is preserved,
                        //     and newly inserted elements follow those with equivalent keys
                        //     already in the container"
            switch (operations.LHRH.method) {
                  case MidiOperation::LHRHMethod::HAND_WIDTH:
                        splitByHandWidth(tracks, it);
                        break;
                  case MidiOperation::LHRHMethod::SPECIFIED_PITCH:
                        splitByFixedPitch(tracks, it);
                        break;
                  }
            }
      }

} // namespace LRHand
} // namespace Ms
