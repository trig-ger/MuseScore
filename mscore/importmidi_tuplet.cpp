#include "importmidi_tuplet.h"
#include "importmidi_tuplet_detect.h"
#include "importmidi_tuplet_filter.h"
#include "importmidi_chord.h"
#include "importmidi_quant.h"
#include "importmidi_inner.h"
#include "libmscore/mscore.h"
#include "preferences.h"

#include <set>


namespace Ms {
namespace MidiTuplet {

const std::map<int, TupletLimits>& tupletsLimits()
      {
      const static std::map<int, TupletLimits> values = {
            {2, {{2, 3}, 1, 2, 2}},
            {3, {{3, 2}, 1, 3, 3}},
            {4, {{4, 3}, 3, 4, 3}},
            {5, {{5, 4}, 3, 4, 4}},
            {7, {{7, 8}, 4, 6, 5}},
            {9, {{9, 8}, 6, 7, 7}}
            };
      return values;
      }

const TupletLimits& tupletLimits(int tupletNumber)
      {
      auto it = tupletsLimits().find(tupletNumber);

      Q_ASSERT_X(it != tupletsLimits().end(), "MidiTuplet::tupletValue", "Unknown tuplet");

      return it->second;
      }

int voiceLimit()
      {
      const auto operations = preferences.midiImportOperations.currentTrackOperations();
      return (operations.useMultipleVoices) ? VOICES : 1;
      }

int tupletVoiceLimit()
      {
      const auto operations = preferences.midiImportOperations.currentTrackOperations();
                  // for multiple voices: one voice is reserved for non-tuplet chords
      return (operations.useMultipleVoices) ? VOICES - 1 : 1;
      }

int averagePitch(const std::map<ReducedFraction,
                                std::multimap<ReducedFraction, MidiChord>::iterator> &chords)
      {
      if (chords.empty())
            return -1;
      int sumPitch = 0;
      int noteCounter = 0;
      for (const auto &chord: chords) {
            const auto &midiNotes = chord.second->second.notes;
            for (const auto &midiNote: midiNotes) {
                  sumPitch += midiNote.pitch;
                  ++noteCounter;
                  }
            }
      return sumPitch / noteCounter;
      }

void sortNotesByPitch(const std::multimap<ReducedFraction, MidiChord>::iterator &startBarChordIt,
                      const std::multimap<ReducedFraction, MidiChord>::iterator &endBarChordIt)
      {
      struct {
            bool operator()(const MidiNote &n1, const MidiNote &n2)
                  {
                  return (n1.pitch > n2.pitch);
                  }
            } pitchComparator;

      for (auto it = startBarChordIt; it != endBarChordIt; ++it) {
            auto &midiNotes = it->second.notes;
            std::sort(midiNotes.begin(), midiNotes.end(), pitchComparator);
            }
      }

void sortTupletsByAveragePitch(std::vector<TupletInfo> &tuplets)
      {
      struct {
            bool operator()(const TupletInfo &t1, const TupletInfo &t2)
                  {
                  return (averagePitch(t1.chords) > averagePitch(t2.chords));
                  }
            } averagePitchComparator;
      std::sort(tuplets.begin(), tuplets.end(), averagePitchComparator);
      }


// find tuplets over which duration lies

std::vector<TupletData>
findTupletsInBarForDuration(
            int voice,
            const ReducedFraction &barStartTick,
            const ReducedFraction &durationOnTime,
            const ReducedFraction &durationLen,
            const std::multimap<ReducedFraction, TupletData> &tupletEvents)
      {
      std::vector<TupletData> tupletsData;
      if (tupletEvents.empty())
            return tupletsData;
      auto tupletIt = tupletEvents.lower_bound(barStartTick);

      while (tupletIt != tupletEvents.end()
                && tupletIt->first < durationOnTime + durationLen) {
            if (tupletIt->second.voice == voice
                        && durationOnTime < tupletIt->first + tupletIt->second.len) {
                              // if tuplet and duration intersect each other
                  auto tupletData = tupletIt->second;
                              // convert tuplet onTime to local bar ticks
                  tupletData.onTime -= barStartTick;
                  tupletsData.push_back(tupletData);
                  }
            ++tupletIt;
            }
      return tupletsData;
      }

std::multimap<ReducedFraction, MidiTuplet::TupletData>::const_iterator
findTupletForTimeRange(
            int voice,
            const ReducedFraction &onTime,
            const ReducedFraction &len,
            const std::multimap<ReducedFraction, TupletData> &tupletEvents)
      {
      Q_ASSERT_X(len >= ReducedFraction(0, 1),
                 "MidiTuplet::findTupletForTimeRange", "Negative length of time range");

      if (tupletEvents.empty())
            return tupletEvents.end();

      auto it = tupletEvents.upper_bound(onTime + len);
      if (it != tupletEvents.begin())
            --it;
      while (true) {
            const auto &tupletData = it->second;
            const auto &tupletOffTime = tupletData.onTime + tupletData.len;
            if (tupletData.voice == voice
                        && onTime >= tupletData.onTime
                        && ((len > ReducedFraction(0, 1)
                             ? onTime + len <= tupletOffTime
                             : onTime < tupletOffTime)
                            )) {
                  return it;
                  }
            if (it == tupletEvents.begin())
                  break;
            --it;
            }
      return tupletEvents.end();
      }

std::multimap<ReducedFraction, MidiTuplet::TupletData>::const_iterator
findTupletContainsTime(
            int voice,
            const ReducedFraction &time,
            const std::multimap<ReducedFraction, TupletData> &tupletEvents)
      {
      return findTupletForTimeRange(voice, time, ReducedFraction(0, 1), tupletEvents);
      }

std::set<std::pair<const ReducedFraction, MidiChord> *>
findTupletChords(const std::vector<TupletInfo> &tuplets)
      {
      std::set<std::pair<const ReducedFraction, MidiChord> *> tupletChords;
      for (const auto &tupletInfo: tuplets) {
            for (const auto &tupletChord: tupletInfo.chords) {
                  auto tupletIt = tupletChord.second;
                  tupletChords.insert(&*tupletIt);
                  }
            }
      return tupletChords;
      }

std::map<std::pair<const ReducedFraction, MidiChord> *, int>
findMappedTupletChords(const std::vector<TupletInfo> &tuplets)
      {
                  // <chord address, tupletIndex>
      std::map<std::pair<const ReducedFraction, MidiChord> *, int> tupletChords;
      for (int i = 0; i != (int)tuplets.size(); ++i) {
            for (const auto &tupletChord: tuplets[i].chords) {
                  auto tupletIt = tupletChord.second;
                  tupletChords.insert({&*tupletIt, i});
                  }
            }
      return tupletChords;
      }

std::list<std::multimap<ReducedFraction, MidiChord>::iterator>
findNonTupletChords(const std::vector<TupletInfo> &tuplets,
                    const std::multimap<ReducedFraction, MidiChord>::iterator &startBarChordIt,
                    const std::multimap<ReducedFraction, MidiChord>::iterator &endBarChordIt)
      {
      const auto tupletChords = findTupletChords(tuplets);
      std::list<std::multimap<ReducedFraction, MidiChord>::iterator> nonTuplets;
      for (auto it = startBarChordIt; it != endBarChordIt; ++it) {
            if (tupletChords.find(&*it) == tupletChords.end())
                  nonTuplets.push_back(it);
            }

      return nonTuplets;
      }

// do quantization before checking

bool hasIntersectionWithChord(
            const ReducedFraction &startTick,
            const ReducedFraction &endTick,
            const ReducedFraction &basicQuant,
            const std::multimap<ReducedFraction, MidiChord>::iterator &chord)
      {
      const auto onTime = Quantize::findQuantizedChordOnTime(*chord, basicQuant);
      const auto offTime = Quantize::findMaxQuantizedOffTime(*chord, basicQuant);
      return (endTick > onTime && startTick < offTime);
      }

// split first tuplet chord, that belong to 2 tuplets, into 2 chords

void splitTupletChord(const std::vector<TupletInfo>::iterator &lastMatch,
                      std::multimap<ReducedFraction, MidiChord> &chords)
      {
      auto &chordEvent = lastMatch->chords.begin()->second;
      MidiChord &prevChord = chordEvent->second;
      const auto onTime = chordEvent->first;
      MidiChord newChord = prevChord;
                        // erase all notes except the first one
      auto beg = newChord.notes.begin();
      newChord.notes.erase(++beg, newChord.notes.end());
                        // erase the first note
      prevChord.notes.erase(prevChord.notes.begin());
      chordEvent = chords.insert({onTime, newChord});
      if (prevChord.notes.isEmpty()) {
                        // normally this should not happen at all because of filtering of tuplets
            qDebug("Tuplets were not filtered correctly: same notes in different tuplets");
            }
      }

void setTupletVoice(
            std::map<ReducedFraction,
                     std::multimap<ReducedFraction, MidiChord>::iterator> &tupletChords,
            int voice)
      {
      for (auto &tupletChord: tupletChords) {
            MidiChord &midiChord = tupletChord.second->second;
            midiChord.voice = voice;
            midiChord.isInTuplet = true;
            }
      }

void splitFirstTupletChords(std::vector<TupletInfo> &tuplets,
                            std::multimap<ReducedFraction, MidiChord> &chords)
      {
      for (auto now = tuplets.begin(); now != tuplets.end(); ++now) {
            auto lastMatch = tuplets.end();
            const auto nowChordIt = now->chords.begin();
            for (auto prev = tuplets.begin(); prev != now; ++prev) {
                  auto prevChordIt = prev->chords.begin();
                  if (now->firstChordIndex == 0
                              && prev->firstChordIndex == 0
                              && nowChordIt->second == prevChordIt->second) {
                        lastMatch = prev;
                        }
                  }
            if (lastMatch != tuplets.end())
                  splitTupletChord(lastMatch, chords);
            }
      }

bool canTupletBeTied(const TupletInfo &tuplet,
                     const std::multimap<ReducedFraction, MidiChord>::iterator &chordIt,
                     const ReducedFraction &startBarTick,
                     const ReducedFraction &basicQuant)
      {
      const auto chordOffTime = MChord::maxNoteOffTime(chordIt->second.notes);
      const auto onTime = Quantize::findQuantizedChordOnTime(*chordIt, basicQuant);
      if (onTime >= tuplet.onTime)
            return false;

      const auto tupletOffTime = Quantize::findMaxQuantizedOffTime(
                                    *chordIt, basicQuant,
                                    tupletLimits(tuplet.tupletNumber).ratio, startBarTick);
                  // if chord offTime is outside this bar or tuplet - discard chord
      if (tupletOffTime < startBarTick
                  || tupletOffTime <= tuplet.onTime
                  || tupletOffTime >= tuplet.onTime + tuplet.len)
            return false;

      const auto offTime = Quantize::findMaxQuantizedOffTime(*chordIt, basicQuant);
      const auto regularError = (chordOffTime - offTime).absValue();
      const auto tupletError = (chordOffTime - tupletOffTime).absValue();

      if (tupletError <= regularError)
            return true;

      return false;
      }

ReducedFraction findPrevBarStart(const ReducedFraction &barStart,
                                 const ReducedFraction &barLen)
      {
      auto prevBarStart = barStart - barLen;
      if (prevBarStart < ReducedFraction(0, 1))
            prevBarStart = ReducedFraction(0, 1);
      return prevBarStart;
      }

struct TiedTuplet
      {
      int tupletIndex;
      int voice;
      std::pair<const ReducedFraction, MidiChord> *chord;  // chord the tuplet is tied with
      };

std::pair<ReducedFraction, ReducedFraction>
tupletInterval(const TupletInfo &tuplet,
               const ReducedFraction &basicQuant)
      {
      ReducedFraction tupletEnd = tuplet.onTime + tuplet.len;

      for (const auto &chord: tuplet.chords) {
            const auto offTime = Quantize::findMaxQuantizedOffTime(*chord.second, basicQuant);
            if (offTime > tupletEnd)
                  tupletEnd = offTime;
            }
      return std::make_pair(tuplet.onTime, tupletEnd);
      }

std::vector<std::pair<ReducedFraction, ReducedFraction> >
findTupletIntervals(const std::vector<TupletInfo> &tuplets,
                    const ReducedFraction &basicQuant)
      {
      std::vector<std::pair<ReducedFraction, ReducedFraction>> tupletIntervals;
      for (const auto &tuplet: tuplets)
            tupletIntervals.push_back(tupletInterval(tuplet, basicQuant));

      return tupletIntervals;
      }

std::vector<TiedTuplet>
findBackTiedTuplets(
            const std::multimap<ReducedFraction, MidiChord> &chords,
            const std::vector<TupletInfo> &tuplets,
            const ReducedFraction &prevBarStart,
            const ReducedFraction &startBarTick,
            const ReducedFraction &basicQuant)
      {
      std::vector<TiedTuplet> tiedTuplets;
      const auto tupletChords = findMappedTupletChords(tuplets);
      const auto tupletIntervals = findTupletIntervals(tuplets, basicQuant);

      for (int i = 0; i != (int)tuplets.size(); ++i) {

            Q_ASSERT_X(!tuplets[i].chords.empty(),
                       "MidiTuplets::findBackTiedTuplets", "Tuplet chords are empty");

            auto chordIt = tuplets[i].chords.begin()->second;
            while (chordIt != chords.begin() && chordIt->first >= prevBarStart) {
                  --chordIt;
                  int voice = chordIt->second.voice;  // voice can be from prev bar also
                  bool used = false;
                              // check: if tuplet 'i' already tied then voices must match
                  for (const auto &t: tiedTuplets) {
                        if (t.tupletIndex == i && t.voice != voice) {
                              used = true;
                              break;
                              }
                        if (t.tupletIndex != i && t.voice == voice) {
                              if (haveIntersection(tupletIntervals[i],
                                                   tupletIntervals[t.tupletIndex])) {
                                    used = true;
                                    break;
                                    }
                              }
                        }
                  if (used)
                        continue;
                              // don't make back tie to the chord in overlapping tuplet
                  const auto tupletIt = tupletChords.find(&*chordIt);
                  if (tupletIt != tupletChords.end()) {
                        const auto onTime1 = tuplets[tupletIt->second].onTime;
                        const auto endTime1 = onTime1 + tuplets[tupletIt->second].len;
                        const auto onTime2 = tuplets[i].onTime;
                        const auto endTime2 = onTime1 + tuplets[i].len;
                        if (endTime1 > onTime2 && onTime1 < endTime2) // tuplet intersection
                              continue;
                        }
                  if (canTupletBeTied(tuplets[i], chordIt, startBarTick, basicQuant)) {
                        tiedTuplets.push_back({i, voice, &*chordIt});
                        break;
                        }
                  }
            }
      return tiedTuplets;
      }

// first tuplet notes with offTime quantization error,
// that is greater for tuplet quant rather than for regular quant,
// are removed from tuplet, except that was the last note

// if noteLen <= tupletLen
//     remove notes with big offTime quant error;
//     and here is a tuning for clearer tuplet processing:
//         if tuplet has only one (first) chord -
//             we can remove all notes and erase tuplet;
//         if tuplet has multiple chords -
//             we should leave at least one note in the first chord

void minimizeOffTimeError(
            std::vector<TupletInfo> &tuplets,
            std::multimap<ReducedFraction, MidiChord> &chords,
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const ReducedFraction &startBarTick,
            const ReducedFraction &basicQuant)
      {
      for (auto it = tuplets.begin(); it != tuplets.end(); ) {
            TupletInfo &tupletInfo = *it;
            const auto firstChord = tupletInfo.chords.begin();
            if (firstChord == tupletInfo.chords.end() || tupletInfo.firstChordIndex != 0) {
                  ++it;
                  continue;
                  }
            auto onTime = firstChord->second->first;
                        // because of tol onTime can be less than start bar tick
            if (onTime < startBarTick)
                  onTime = startBarTick;
            MidiChord &midiChord = firstChord->second->second;
            auto &notes = midiChord.notes;

            std::vector<int> removedIndexes;
            std::vector<int> leavedIndexes;
            for (int i = 0; i != notes.size(); ++i) {
                  const auto &note = notes[i];
                  if (note.offTime - onTime <= tupletInfo.len) {
                        if ((tupletInfo.chords.size() == 1
                                    && notes.size() > (int)removedIndexes.size())
                                 || (tupletInfo.chords.size() > 1
                                    && notes.size() > (int)removedIndexes.size() + 1))
                              {
                              const auto tupletError = Quantize::findOffTimeQuantError(
                                                *firstChord->second, note.offTime, basicQuant,
                                                tupletLimits(tupletInfo.tupletNumber).ratio, startBarTick);
                              const auto regularError = Quantize::findOffTimeQuantError(
                                                *firstChord->second, note.offTime, basicQuant);

                              if (tupletError > regularError) {
                                    removedIndexes.push_back(i);
                                    continue;
                                    }
                              }
                        }
                  leavedIndexes.push_back(i);
                  }
            if (!removedIndexes.empty()) {
                  MidiChord newTupletChord;
                  for (int i: leavedIndexes)
                        newTupletChord.notes.push_back(notes[i]);

                  QList<MidiNote> newNotes;
                  for (int i: removedIndexes)
                        newNotes.push_back(notes[i]);
                  notes = newNotes;
                  nonTuplets.push_back(firstChord->second);
                  if (!newTupletChord.notes.empty())
                        firstChord->second = chords.insert({onTime, newTupletChord});
                  else {
                        tupletInfo.chords.erase(tupletInfo.chords.begin());
                        if (tupletInfo.chords.empty()) {
                              it = tuplets.erase(it);   // remove tuplet without chords
                              continue;
                              }
                        }
                  }
            ++it;
            }
      }

void addChordsBetweenTupletNotes(
            std::vector<TupletInfo> &tuplets,
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const ReducedFraction &startBarTick,
            const ReducedFraction &basicQuant)
      {
      for (TupletInfo &tuplet: tuplets) {
            for (auto it = nonTuplets.begin(); it != nonTuplets.end(); ) {
                  const auto &chordIt = *it;
                  const auto &onTime = chordIt->first;
                  if (onTime > tuplet.onTime
                              && hasIntersectionWithChord(tuplet.onTime, tuplet.onTime + tuplet.len,
                                                          basicQuant, chordIt)) {
                        const auto tupletRatio = tupletLimits(tuplet.tupletNumber).ratio;

                        auto tupletError = Quantize::findOnTimeQuantError(
                                                  *chordIt, basicQuant, tupletRatio, startBarTick);
                        auto regularError = Quantize::findOnTimeQuantError(*chordIt, basicQuant);

                        const auto offTime = MChord::maxNoteOffTime(chordIt->second.notes);
                        if (offTime < tuplet.onTime + tuplet.len) {
                              tupletError += Quantize::findOffTimeQuantError(
                                                *chordIt, offTime, basicQuant,
                                                tupletRatio, startBarTick);
                              regularError += Quantize::findOffTimeQuantError(
                                                *chordIt, offTime, basicQuant);
                              }
                        if (tupletError < regularError) {
                              tuplet.chords.insert({onTime, chordIt});
                              it = nonTuplets.erase(it);
                              continue;
                              }
                        }
                  ++it;
                  }
            }
      }

std::pair<ReducedFraction, ReducedFraction>
chordInterval(const std::pair<const ReducedFraction, MidiChord> &chord,
              const ReducedFraction &basicQuant)
      {
      const auto onTime = Quantize::findMinQuantizedOnTime(chord, basicQuant);
      const auto offTime = Quantize::findMaxQuantizedOffTime(chord, basicQuant);
      return std::make_pair(onTime, offTime);
      }

void setTupletVoices(
            std::vector<TupletInfo> &tuplets,
            std::set<int> &pendingTuplets,
            std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tupletIntervals,
            const ReducedFraction &basicQuant)
      {
      int limit = tupletVoiceLimit();
      int voice = 0;
      while (!pendingTuplets.empty() && voice < limit) {
            for (auto it = pendingTuplets.begin(); it != pendingTuplets.end(); ) {
                  int i = *it;
                  const auto interval = tupletInterval(tuplets[i], basicQuant);
                  if (!haveIntersection(interval, tupletIntervals[voice])) {
                        setTupletVoice(tuplets[i].chords, voice);
                        tupletIntervals[voice].push_back(interval);
                        it = pendingTuplets.erase(it);
                        continue;
                        }
                  ++it;
                  }
            ++voice;
            }
      }

void setNonTupletVoices(
            std::set<std::pair<const ReducedFraction, MidiChord> *> &pendingNonTuplets,
            const std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tupletIntervals,
            const ReducedFraction &basicQuant)
      {
      const int limit = voiceLimit();
      int voice = 0;
      while (!pendingNonTuplets.empty() && voice < limit) {
            for (auto it = pendingNonTuplets.begin(); it != pendingNonTuplets.end(); ) {
                  auto chord = *it;
                  const auto interval = chordInterval(*chord, basicQuant);
                  const auto fit = tupletIntervals.find(voice);
                  if (fit == tupletIntervals.end() || !haveIntersection(interval, fit->second)) {
                        chord->second.voice = voice;
                        it = pendingNonTuplets.erase(it);
                                    // don't insert chord interval here
                        continue;
                        }
                  ++it;
                  }
            ++voice;
            }
      }

void removeUnusedTuplets(
            std::vector<TupletInfo> &tuplets,
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            std::set<int> &pendingTuplets,
            std::set<std::pair<const ReducedFraction, MidiChord> *> &pendingNonTuplets)
      {
      if (pendingTuplets.empty())
            return;

      std::vector<TupletInfo> newTuplets;
      for (int i = 0; i != (int)tuplets.size(); ++i) {
            if (pendingTuplets.find(i) == pendingTuplets.end()) {
                  newTuplets.push_back(tuplets[i]);
                  }
            else {
                  for (const auto &chord: tuplets[i].chords) {
                        nonTuplets.push_back(chord.second);
                        pendingNonTuplets.insert(&*chord.second);
                        }
                  }
            }
      pendingTuplets.clear();
      std::swap(tuplets, newTuplets);
      }


#ifdef QT_DEBUG

bool haveTupletsEmptyChords(const std::vector<TupletInfo> &tuplets)
      {
      for (const auto &tuplet: tuplets) {
            if (tuplet.chords.empty())
                  return true;
            }
      return false;
      }

bool doTupletChordsHaveSameVoice(const std::vector<TupletInfo> &tuplets)
      {
      for (const auto &tuplet: tuplets) {
            auto it = tuplet.chords.cbegin();
            const int voice = it->second->second.voice;
            ++it;
            for ( ; it != tuplet.chords.cend(); ++it) {
                  if (it->second->second.voice != voice)
                        return false;
                  }
            }
      return true;
      }

// back tied tuplets are not checked here

bool haveOverlappingVoices(
            const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const std::vector<TupletInfo> &tuplets,
            const ReducedFraction &basicQuant,
            const std::vector<TiedTuplet> &backTiedTuplets = std::vector<TiedTuplet>())
      {
                  // <voice, intervals>
      std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> intervals;

      for (const auto &tuplet: tuplets) {
            const int voice = tuplet.chords.begin()->second->second.voice;
            const auto interval = std::make_pair(tuplet.onTime, tuplet.onTime + tuplet.len);
            if (haveIntersection(interval, intervals[voice]))
                  return true;
            else
                  intervals[voice].push_back(interval);
            }

      for (const auto &chord: nonTuplets) {
            const int voice = chord->second.voice;
            const auto interval = chordInterval(*chord, basicQuant);
            if (haveIntersection(interval, intervals[voice])) {
                  bool flag = false;      // if chord is tied then it can intersect tuplet
                  for (const TiedTuplet &tiedTuplet: backTiedTuplets) {
                        if (tiedTuplet.chord == (&*chord) && tiedTuplet.voice == voice) {
                              flag = true;
                              break;
                              }
                        }
                  if (!flag)
                        return true;
                  }
            }

      return false;
      }

bool doTupletsHaveCommonChords(const std::vector<TupletInfo> &tuplets)
      {
      if (tuplets.empty())
            return false;
      std::set<std::pair<const ReducedFraction, MidiChord> *> chordsI;
      for (const auto &tuplet: tuplets) {
            for (const auto &chord: tuplet.chords) {
                  if (chordsI.find(&*chord.second) != chordsI.end())
                        return true;
                  chordsI.insert(&*chord.second);
                  }
            }
      return false;
      }

#endif


std::vector<std::pair<ReducedFraction, ReducedFraction> >
findNonTupletIntervals(
            const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const ReducedFraction &basicQuant)
      {
      std::vector<std::pair<ReducedFraction, ReducedFraction>> nonTupletIntervals;
      for (const auto &nonTuplet: nonTuplets) {
            nonTupletIntervals.push_back(chordInterval(*nonTuplet, basicQuant));
            }
      return nonTupletIntervals;
      }

std::set<int> findPendingTuplets(const std::vector<TupletInfo> &tuplets)
      {
      std::set<int> pendingTuplets;       // tuplet indexes
      for (int i = 0; i != (int)tuplets.size(); ++i) {
            pendingTuplets.insert(i);
            }
      return pendingTuplets;
      }

std::set<std::pair<const ReducedFraction, MidiChord> *>
findPendingNonTuplets(
            const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets)
      {
      std::set<std::pair<const ReducedFraction, MidiChord> *> pendingNonTuplets;
      for (const auto &c: nonTuplets) {
            pendingNonTuplets.insert(&*c);
            }
      return pendingNonTuplets;
      }

int findTupletWithChord(const MidiChord &midiChord,
                        const std::vector<TupletInfo> &tuplets)
      {
      for (int i = 0; i != (int)tuplets.size(); ++i) {
            for (const auto &chord: tuplets[i].chords) {
                  if (&(chord.second->second) == &midiChord)
                        return i;
                  }
            }
      return -1;
      }

std::vector<std::pair<int, int> >
findForTiedTuplets(
            const std::vector<TupletInfo> &tuplets,
            const std::vector<TiedTuplet> &tiedTuplets,
            const std::set<std::pair<const ReducedFraction, MidiChord> *> &pendingNonTuplets,
            const ReducedFraction &startBarTick)
      {
      std::vector<std::pair<int, int>> forTiedTuplets;  // <tuplet index, voice to assign>

      for (const TiedTuplet &tuplet: tiedTuplets) {
                        // only for chords in the current bar (because of tol some can be outside)
            if (tuplet.chord->first < startBarTick)
                  continue;
            if (pendingNonTuplets.find(tuplet.chord) == pendingNonTuplets.end()) {
                  const int i = findTupletWithChord(tuplet.chord->second, tuplets);
                  if (i != -1)
                        forTiedTuplets.push_back({i, tuplet.voice});
                  }
            }
      return forTiedTuplets;
      }


#ifdef QT_DEBUG

bool areAllElementsUnique(
            const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets)
      {
      std::set<std::pair<const ReducedFraction, MidiChord> *> chords;
      for (const auto &chord: nonTuplets) {
            if (chords.find(&*chord) == chords.end())
                  chords.insert(&*chord);
            else
                  return false;
            }
      return true;
      }

size_t chordCount(
            const std::vector<TupletInfo> &tuplets,
            const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets)
      {
      size_t sum = nonTuplets.size();
      for (const auto &tuplet: tuplets) {
            sum += tuplet.chords.size();
            }
      return sum;
      }

#endif


// for the case !useMultipleVoices

void excludeExtraVoiceTuplets(
            std::vector<TupletInfo> &tuplets,
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const ReducedFraction &basicQuant)
      {
                  // remove overlapping tuplets
      size_t sz = tuplets.size();
      if (sz == 0)
            return;
      while (true) {
            bool change = false;
            for (size_t i = 0; i < sz - 1; ++i) {
                  const auto interval1 = tupletInterval(tuplets[i], basicQuant);
                  for (size_t j = i + 1; j < sz; ++j) {
                        const auto interval2 = tupletInterval(tuplets[j], basicQuant);
                        if (haveIntersection(interval1, interval2)) {
                              --sz;
                              if (j < sz)
                                    tuplets[j] = tuplets[sz];
                              --sz;
                              if (i < sz)
                                    tuplets[i] = tuplets[sz];
                              change = true;
                              break;
                              }
                        }
                  if (change)
                        break;
                  }
            if (!change || sz == 0)
                  break;
            }

      if (sz > 0) {     // remove tuplets that are overlapped with non-tuplets
            const auto nonTupletIntervals = findNonTupletIntervals(nonTuplets, basicQuant);

            for (size_t i = 0; i < sz; ) {
                  const auto interval = tupletInterval(tuplets[i], basicQuant);
                  if (haveIntersection(interval, nonTupletIntervals)) {
                        for (const auto &chord: tuplets[i].chords)
                              nonTuplets.push_back(chord.second);
                        --sz;
                        if (i < sz) {
                              tuplets[i] = tuplets[sz];
                              continue;
                              }
                        }
                  ++i;
                  }
            }

      Q_ASSERT_X(areAllElementsUnique(nonTuplets),
                 "MIDI tuplets: excludeExtraVoiceTuplets", "non unique chords in non-tuplets");

      tuplets.resize(sz);
      }

void assignVoices(
            std::vector<TupletInfo> &tuplets,
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const std::vector<TiedTuplet> &backTiedTuplets,
            const ReducedFraction &startBarTick,
            const ReducedFraction &basicQuant)
      {
#ifdef QT_DEBUG
      size_t oldChordCount = chordCount(tuplets, nonTuplets);
#endif
      Q_ASSERT_X(!haveTupletsEmptyChords(tuplets),
                 "MIDI tuplets: assignVoices", "Empty tuplet chords");

      auto pendingTuplets = findPendingTuplets(tuplets);
      auto pendingNonTuplets = findPendingNonTuplets(nonTuplets);
      const auto forTiedTuplets = findForTiedTuplets(tuplets, backTiedTuplets,
                                                     pendingNonTuplets, startBarTick);
      std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> tupletIntervals;

      for (const auto &t: forTiedTuplets) {
            const int i = t.first;
            const int voice = t.second;
            setTupletVoice(tuplets[i].chords, voice);
                        // remove tuplets with already set voices
            pendingTuplets.erase(i);
            tupletIntervals[voice].push_back(tupletInterval(tuplets[i], basicQuant));
            }

      for (const TiedTuplet &tuplet: backTiedTuplets) {
            setTupletVoice(tuplets[tuplet.tupletIndex].chords, tuplet.voice);
            pendingTuplets.erase(tuplet.tupletIndex);
            tupletIntervals[tuplet.voice].push_back(
                              tupletInterval(tuplets[tuplet.tupletIndex], basicQuant));
                        // set for-tied chords
                        // some chords can be the same as in forTiedTuplets

                  // only for chords in the current bar (because of tol some can be outside)
            if (tuplet.chord->first < startBarTick)
                  continue;
            tuplet.chord->second.voice = tuplet.voice;
                        // remove chords with already set voices
            pendingNonTuplets.erase(tuplet.chord);
            }

      {
      setTupletVoices(tuplets, pendingTuplets, tupletIntervals, basicQuant);

      Q_ASSERT_X((voiceLimit() == 1) ? pendingTuplets.empty() : true,
                 "MIDI tuplets: assignVoices", "Unused tuplets for the case !useMultipleVoices");

      removeUnusedTuplets(tuplets, nonTuplets, pendingTuplets, pendingNonTuplets);
      setNonTupletVoices(pendingNonTuplets, tupletIntervals, basicQuant);
      }

      Q_ASSERT_X(pendingNonTuplets.empty(),
                 "MIDI tuplets: assignVoices", "Unused non-tuplets");
      Q_ASSERT_X(!haveTupletsEmptyChords(tuplets),
                 "MIDI tuplets: assignVoices", "Empty tuplet chords");
      Q_ASSERT_X(doTupletChordsHaveSameVoice(tuplets),
                 "MIDI tuplets: assignVoices", "Tuplet chords have different voices");
      Q_ASSERT_X(!haveOverlappingVoices(nonTuplets, tuplets, basicQuant, backTiedTuplets),
                 "MIDI tuplets: assignVoices", "Overlapping tuplets of the same voice");
#ifdef QT_DEBUG
      size_t newChordCount = chordCount(tuplets, nonTuplets);
#endif
      Q_ASSERT_X(oldChordCount == newChordCount,
                 "MIDI tuplets: assignVoices", "Chord count is not preserved");
      }

std::vector<TupletData> convertToData(const std::vector<TupletInfo> &tuplets)
      {
      std::vector<TupletData> tupletsData;
      for (const auto &tupletInfo: tuplets) {
            MidiTuplet::TupletData tupletData = {tupletInfo.chords.begin()->second->second.voice,
                                                 tupletInfo.onTime,
                                                 tupletInfo.len,
                                                 tupletInfo.tupletNumber,
                                                 {}};
            tupletsData.push_back(tupletData);
            }
      return tupletsData;
      }

// check is the chord already in tuplet in prev bar or division
// it's possible because we use (startDivTick - tol) as a start tick

std::multimap<ReducedFraction, MidiChord>::iterator
findTupletFreeChord(
            const std::multimap<ReducedFraction, MidiChord>::iterator &startChordIt,
            const std::multimap<ReducedFraction, MidiChord>::iterator &endChordIt,
            const ReducedFraction &startDivTick)
      {
      auto result = startChordIt;
      for (auto it = startChordIt; it != endChordIt && it->first < startDivTick; ++it) {
            if (it->second.isInTuplet)
                  result = std::next(it);
            }
      return result;
      }

void resetTupletVoices(std::vector<TupletInfo> &tuplets)
      {
      for (auto &tuplet: tuplets) {
            for (auto &chord: tuplet.chords) {
                  MidiChord &midiChord = chord.second->second;
                  midiChord.voice = 0;
                  }
            }
      }

void findTupletQuantizedOffTime(
            std::vector<TupletInfo> &tuplets,
            const ReducedFraction &barStart,
            const ReducedFraction &barEnd,
            const ReducedFraction &basicQuant)
      {
      for (auto &tuplet: tuplets) {
            const auto tupletNoteLen = tuplet.len / tuplet.tupletNumber;
            const auto tupletRatio = tupletLimits(tuplet.tupletNumber).ratio;
            for (auto it = tuplet.chords.begin(); it != tuplet.chords.end(); ++it) {
                  MidiChord &midiChord = it->second->second;
                  for (auto &note: midiChord.notes) {
                        if (note.staccato) {
                                    // decrease tuplet error by enlarging staccato notes:
                                    // make note.len = tuplet note length
                              auto offTime = Quantize::findQuantizedNoteOffTime(
                                          *it->second, it->first + tupletNoteLen, basicQuant,
                                          tupletRatio, barStart);
                              auto next = std::next(it);
                              if (next != tuplet.chords.end()) {
                                    const auto nextOnTime = next->second->second.quantizedOnTime;

                                    Q_ASSERT_X(nextOnTime != ReducedFraction(-1, 1),
                                         "MidiTuplet::findTupletQuantizedOffTime",
                                         "Tuplet onTime is not quantized but it should at this time");

                                    if (offTime > nextOnTime)
                                          offTime = nextOnTime;
                                    }
                              note.quantizedOffTime = offTime;
                              }
                        else {
                              if (note.offTime <= tuplet.onTime + tuplet.len) {
                                    note.quantizedOffTime = Quantize::findQuantizedNoteOffTime(
                                                *it->second, note.offTime, basicQuant,
                                                tupletRatio, barStart);
                                    }
                              else if (note.offTime == ReducedFraction(-1, 1) // unset yet
                                              && note.offTime <= barEnd) {    // belongs to this bar
                                    note.quantizedOffTime = Quantize::findQuantizedNoteOffTime(
                                                       *it->second, note.offTime, basicQuant);
                                    }
                              // outside bar quantization cases will be considered later
                              }
                        }
                  }
            }
      }

void markStaccatoTupletNotes(std::vector<TupletInfo> &tuplets)
      {
      for (auto &tuplet: tuplets) {
            for (const auto &staccato: tuplet.staccatoChords) {
                  const auto it = tuplet.chords.find(staccato.first);
                  if (it != tuplet.chords.end()) {
                        MidiChord &midiChord = it->second->second;
                        midiChord.notes[staccato.second].staccato = true;
                        }
                  }
            tuplet.staccatoChords.clear();
            }
      }

// if notes with staccato were in tuplets previously - remove staccato

void cleanStaccatoOfNonTuplets(
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets)
      {
      for (auto &nonTuplet: nonTuplets) {
            for (auto &note: nonTuplet->second.notes) {
                  if (note.staccato)
                        note.staccato = false;
                  }
            }
      }

void findTupletQuantizedOnTime(
            std::vector<TupletInfo> &tuplets,
            const ReducedFraction &startBarTick,
            const ReducedFraction &basicQuant)
      {
      for (auto &tuplet: tuplets) {
            for (auto &chord: tuplet.chords) {
                  if (chord.first < startBarTick) {
                        chord.second->second.quantizedOnTime = startBarTick;
                        }
                  else {
                        chord.second->second.quantizedOnTime = Quantize::findQuantizedChordOnTime(
                                          *chord.second, basicQuant,
                                          tupletLimits(tuplet.tupletNumber).ratio, startBarTick);
                        }

                  Q_ASSERT_X(chord.second->second.quantizedOnTime >= tuplet.onTime,
                             "MidiTuplet::findTupletQuantizedOnTime",
                             "Chord onTime value is less than tuplet begin time");
                  }
            }
      }

void findNonTupletQuantizedOnTime(
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const ReducedFraction &basicQuant)
      {
      for (auto &nonTuplet: nonTuplets) {
            nonTuplet->second.quantizedOnTime = Quantize::findQuantizedChordOnTime(
                                                      *nonTuplet, basicQuant);
            }
      }

void findNonTupletQuantizedOffTime(
            std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
            const ReducedFraction &basicQuant,
            const ReducedFraction &endBarTick)
      {
      for (auto &nonTuplet: nonTuplets) {
            for (auto &note: nonTuplet->second.notes) {
                        // if offTime is already set or belongs to another bar - go to next note
                  if (note.quantizedOffTime == ReducedFraction(-1, 1)
                              && note.offTime <= endBarTick) {
                        note.quantizedOffTime = Quantize::findQuantizedNoteOffTime(
                                          *nonTuplet, note.offTime, basicQuant);
                        }
                  // outside bar quantization cases will be considered later
                  }
            }
      }

void findTiedQuantizedOffTime(const std::vector<TiedTuplet> &backTiedTuplets,
                              const std::vector<TupletInfo> &tuplets,
                              const ReducedFraction &startBarTick,
                              const ReducedFraction &basicQuant)
      {
      for (const TiedTuplet &tuplet: backTiedTuplets) {
            for (auto &note: tuplet.chord->second.notes) {
                  if (note.offTime > tuplets[tuplet.tupletIndex].onTime) {

                        Q_ASSERT_X(note.quantizedOffTime == ReducedFraction(-1, 1),
                                   "MidiTuplet::findTiedQuantizedOffTime",
                                   "Note quantized off time is already set");

                        note.quantizedOffTime = Quantize::findQuantizedNoteOffTime(
                                    *tuplet.chord, note.offTime, basicQuant,
                                    tupletLimits(tuplets[tuplet.tupletIndex].tupletNumber).ratio,
                                    startBarTick);
                        }
                  }
            }
      }

std::vector<TupletData> findTuplets(
            const ReducedFraction &startBarTick,
            const ReducedFraction &endBarTick,
            const ReducedFraction &barFraction,
            std::multimap<ReducedFraction, MidiChord> &chords,
            const ReducedFraction &basicQuant)
      {
      if (chords.empty() || startBarTick >= endBarTick)     // invalid cases
            return std::vector<TupletData>();
      const auto operations = preferences.midiImportOperations.currentTrackOperations();
      if (!operations.tuplets.doSearch)
            return std::vector<TupletData>();
      auto startBarChordIt = MChord::findFirstChordInRange(chords, startBarTick, endBarTick);
      startBarChordIt = findTupletFreeChord(startBarChordIt, chords.end(), startBarTick);
      if (startBarChordIt == chords.end())      // no chords in this bar
            return std::vector<TupletData>();

      const auto endBarChordIt = chords.lower_bound(endBarTick);
                  // update start chord: use chords with onTime >= (start bar tick - tol)
      const auto tol = basicQuant / 2;
      startBarChordIt = MChord::findFirstChordInRange(chords, startBarTick - tol, endBarTick);

      std::vector<TupletInfo> tuplets = detectTuplets(chords, barFraction, startBarTick, tol,
                                                      endBarChordIt, startBarChordIt, basicQuant);
      filterTuplets(tuplets, basicQuant);

            // later notes will be sorted and their indexes become invalid
            // so assign staccato information to notes now
      markStaccatoTupletNotes(tuplets);

            // because of tol for non-tuplets we should use only chords with onTime >= bar start
      auto startNonTupletChordIt = startBarChordIt;
      while (startNonTupletChordIt->first < startBarTick)
            ++startNonTupletChordIt;
      auto nonTuplets = findNonTupletChords(tuplets, startNonTupletChordIt, endBarChordIt);
      if (tupletVoiceLimit() == 1)
            excludeExtraVoiceTuplets(tuplets, nonTuplets, basicQuant);

      resetTupletVoices(tuplets);  // because of tol some chords may have non-zero voices
      addChordsBetweenTupletNotes(tuplets, nonTuplets, startBarTick, basicQuant);
      sortNotesByPitch(startBarChordIt, endBarChordIt);
      sortTupletsByAveragePitch(tuplets);

      if (operations.useMultipleVoices) {
            splitFirstTupletChords(tuplets, chords);
            minimizeOffTimeError(tuplets, chords, nonTuplets, startBarTick, basicQuant);
            }

      cleanStaccatoOfNonTuplets(nonTuplets);

      Q_ASSERT_X(!doTupletsHaveCommonChords(tuplets),
                 "MIDI tuplets: findTuplets", "Tuplets have common chords but they shouldn't");
      Q_ASSERT_X((voiceLimit() == 1)
                        ? !haveOverlappingVoices(nonTuplets, tuplets, basicQuant)
                        : true,
                 "MIDI tuplets: findTuplets",
                 "Overlapping tuplet and non-tuplet voices for the case !useMultipleVoices");

      const auto prevBarStart = findPrevBarStart(startBarTick, endBarTick - startBarTick);
      const auto backTiedTuplets = findBackTiedTuplets(chords, tuplets, prevBarStart,
                                                       startBarTick, basicQuant);
      assignVoices(tuplets, nonTuplets, backTiedTuplets, startBarTick, basicQuant);

      findTiedQuantizedOffTime(backTiedTuplets, tuplets, startBarTick, basicQuant);
      findTupletQuantizedOnTime(tuplets, startBarTick, basicQuant);
      findTupletQuantizedOffTime(tuplets, startBarTick, endBarTick, basicQuant);
      findNonTupletQuantizedOnTime(nonTuplets, basicQuant);
      findNonTupletQuantizedOffTime(nonTuplets, basicQuant, endBarTick);

      return convertToData(tuplets);
      }

std::multimap<ReducedFraction, TupletData>
findAllTuplets(std::multimap<ReducedFraction, MidiChord> &chords,
               const TimeSigMap *sigmap,
               const ReducedFraction &lastTick,
               const ReducedFraction &basicQuant)
      {
      std::multimap<ReducedFraction, TupletData> tupletEvents;
      ReducedFraction startBarTick = {0, 1};

      for (int i = 1;; ++i) {       // iterate over all measures by indexes
            const auto endBarTick = ReducedFraction::fromTicks(sigmap->bar2tick(i, 0));
            const auto barFraction = ReducedFraction(sigmap->timesig(startBarTick.ticks()).timesig());
            const auto tuplets = findTuplets(startBarTick, endBarTick, barFraction, chords, basicQuant);
            for (const auto &tupletData: tuplets)
                  tupletEvents.insert({tupletData.onTime, tupletData});
            if (endBarTick > lastTick)
                  break;
            startBarTick = endBarTick;
            }
      return tupletEvents;
      }

// tuplets with no chords are removed
// tuplets with single chord with chord.onTime = tuplet.onTime
//    and chord.len = tuplet.len are removed as well

void removeEmptyTuplets(MTrack &track)
      {
      if (track.tuplets.empty())
            return;
      for (auto it = track.tuplets.begin(); it != track.tuplets.end(); ) {
            const auto &tupletData = it->second;
            bool ok = false;
            for (const auto &chord: track.chords) {
                  if (chord.first >= tupletData.onTime + tupletData.len)
                        break;
                  if (tupletData.voice != chord.second.voice)
                        continue;
                  const ReducedFraction &onTime = chord.first;
                  if (onTime >= tupletData.onTime
                              && onTime < tupletData.onTime + tupletData.len) {
                                    // tuplet contains at least one chord
                                    // check now for notes with len == tupletData.len
                        if (onTime == tupletData.onTime) {
                              for (const auto &note: chord.second.notes) {
                                    if (note.offTime - onTime < tupletData.len) {
                                          ok = true;
                                          break;
                                          }
                                    }
                              }
                        else {
                              ok = true;
                              break;
                              }
                        }
                  }
            if (!ok) {
                  it = track.tuplets.erase(it);
                  continue;
                  }
            ++it;
            }
      }

} // namespace MidiTuplet
} // namespace Ms
