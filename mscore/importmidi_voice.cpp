#include "importmidi_voice.h"
#include "importmidi_fraction.h"
#include "importmidi_chord.h"
#include "importmidi_tuplet.h"


namespace Ms {
namespace MidiVoice {


int maxTupletVoice(const std::vector<TupletInfo> &tuplets)
      {
      int maxVoice = 0;
      for (const TupletInfo &tupletInfo: tuplets) {
            if (tupletInfo.chords.empty())
                  continue;
            const int tupletVoice = tupletInfo.chords.begin()->second->second.voice;
            if (tupletVoice > maxVoice)
                  maxVoice = tupletVoice;
            }
      return maxVoice;
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

void setTupletVoice(std::map<ReducedFraction, std::multimap<ReducedFraction, MidiChord>::iterator> &tupletChords,
                    int voice)
      {
      for (auto &tupletChord: tupletChords) {
            MidiChord &midiChord = tupletChord.second->second;
            midiChord.voice = voice;
            }
      }

void setNonTupletVoices(std::vector<TupletInfo> &tuplets,
                        const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
                        const ReducedFraction &regularRaster)
      {
      const auto tuplet = findClosestTuplet(nonTuplets, tuplets);
      const int maxVoice = maxTupletVoice(tuplets) + 1;
      const int tupletVoice = (tuplet.chords.empty())
                  ? 0 : tuplet.chords.begin()->second->second.voice;
      for (auto &chord: nonTuplets) {
            if (hasIntersectionWithChord(tuplet.onTime, tuplet.onTime + tuplet.len,
                                         regularRaster, chord))
                  chord->second.voice = maxVoice;
            else
                  chord->second.voice = tupletVoice;
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

// the input tuplets should be filtered (for mutual validity)
// the output voices can be larger than VOICES - 1
// it will be handled later by merging voices

void separateTupletVoices(std::vector<TupletInfo> &tuplets,
                          const std::multimap<ReducedFraction, MidiChord>::iterator &startBarChordIt,
                          const std::multimap<ReducedFraction, MidiChord>::iterator &endBarChordIt,
                          std::multimap<ReducedFraction, MidiChord> &chords)
      {
      if (tuplets.empty())
            return;
      sortNotesByPitch(startBarChordIt, endBarChordIt);
      sortTupletsByAveragePitch(tuplets);
      splitFirstTupletChords(tuplets, chords);

      for (auto now = tuplets.begin(); now != tuplets.end(); ++now) {
            int voice = 0;
            for (auto prev = tuplets.begin(); prev != now; ++prev) {
                              // check is <now> tuplet go over <prev> tuplet
                  if (now->onTime + now->len > prev->onTime
                              && now->onTime < prev->onTime + prev->len)
                        ++voice;
                  }
            setTupletVoice(now->chords, voice);
            }
      }

void shrinkVoices(const std::multimap<ReducedFraction, MidiChord>::iterator startBarChordIt,
                  const std::multimap<ReducedFraction, MidiChord>::iterator endBarChordIt,
                  const std::set<int> &notMovableVoices)
      {
      int shift = 0;
      for (int voice = 0; voice < VOICES; ++voice) {
            bool voiceInUse = false;
            for (auto it = startBarChordIt; it != endBarChordIt; ++it) {
                  if (it->second.voice == voice) {
                        voiceInUse = true;
                        break;
                        }
                  }
            if (shift > 0 && voiceInUse) {
                  for (auto it = startBarChordIt; it != endBarChordIt; ++it) {
                        if (it->second.voice == voice
                                    && notMovableVoices.find(voice) == notMovableVoices.end()) {
                              int counter = 0;
                              while (counter < shift && it->second.voice > 0) {
                                    --it->second.voice;
                                    if (notMovableVoices.find(it->second.voice) == notMovableVoices.end())
                                          ++counter;
                                    }
                              }
                        }
                  }
            if (!voiceInUse)
                  ++shift;
            }
      }

// fast enough for small vector sizes

bool haveIntersection(const std::vector<std::pair<ReducedFraction, ReducedFraction>> &v1,
                      const std::vector<std::pair<ReducedFraction, ReducedFraction>> &v2)
      {
      if (v1.empty() || v2.empty())
            return false;
      for (const auto &r1: v1) {
            for (const auto &r2: v2) {
                  if (r1.second > r2.first && r1.first < r2.second)
                        return true;
                  }
            }
      return false;
      }

// we don't use interval trees here because the interval count is small

void mergeVoices(std::vector<TupletInfo> &tuplets,
                 std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
                 const std::set<int> &notMovableVoices)
      {
                  // <voice, intervals>
      std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> intervals;
      std::map<int, std::vector<std::multimap<ReducedFraction, MidiChord>::iterator>> chords;

      for (const auto &tuplet: tuplets) {
            if (tuplet.chords.empty())
                  continue;
            int tupletVoice = tuplet.chords.begin()->second->second.voice;
            intervals[tupletVoice].push_back({tuplet.onTime, tuplet.onTime + tuplet.len});
            for (const auto &chord: tuplet.chords)
                  chords[tupletVoice].push_back(chord.second);
            }
      for (const auto &nonTuplet: nonTuplets) {
            if (nonTuplet->second.notes.empty())
                  continue;
            intervals[nonTuplet->second.voice].push_back(
                  {nonTuplet->first, nonTuplet->first + MChord::maxNoteLen(nonTuplet->second.notes)});
            chords[nonTuplet->second.voice].push_back(nonTuplet);
            }

      auto prevEnd = intervals.end();
      --prevEnd;
      for (auto i1 = intervals.begin(); i1 != prevEnd; ++i1) {
            auto i2 = i1;
            ++i2;
            for (; i2 != intervals.end(); ++i2) {
                  if (notMovableVoices.find(i2->first) != notMovableVoices.end())
                        continue;
                  if (!haveIntersection(i1->second, i2->second)) {
                        i1->second.insert(i1->second.end(), i2->second.begin(), i2->second.end());
                        i2->second.clear();
                        for (auto &chord: chords[i2->first])
                              chord->second.voice = i1->first;
                        chords[i1->first].insert(chords[i1->first].end(),
                                              chords[i2->first].begin(), chords[i2->first].end());
                        chords[i2->first].clear();
                        }
                  }
            }
      }

void adjustNonTupletVoices(std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
                           const std::vector<TupletInfo> &tuplets)
      {
      int maxNonTupletVoice = maxTupletVoice(tuplets) + 1;
      std::list<std::multimap<ReducedFraction, MidiChord>::iterator> maxNonTuplets;
      for (const auto &chord: nonTuplets) {
            if (chord->second.voice == maxNonTupletVoice)
                  maxNonTuplets.push_back(chord);
            }
      if (maxNonTuplets.empty())
            return;
      int maxPitch = averagePitch(maxNonTuplets);

      for (auto &chord: nonTuplets) {
            int &chordVoice = chord->second.voice;
            if (chordVoice != maxNonTupletVoice) {
                  int tupletPitch = 0;
                  for (const TupletInfo &tuplet: tuplets) {
                        if (tuplet.chords.empty())
                              continue;
                        if (tuplet.chords.begin()->second->second.voice == chordVoice)
                              tupletPitch = averagePitch(tuplet.chords);
                        }
                  const int avgPitch = averageChordPitch(chord);
                  const int tupletPitchDiff = qAbs(avgPitch - tupletPitch);
                  const int maxPitchDiff = qAbs(avgPitch - maxPitch);
                  if (maxPitchDiff < tupletPitchDiff)
                        chordVoice = maxNonTupletVoice;
                  }
            }
      }

bool isVoiceChangeAllowed(const std::vector<TupletInfo> &tuplets,
                          const std::set<int> &changedTuplets,
                          int newVoice)
      {
      for (int i = 0; i != (int)tuplets.size(); ++i) {
            if (tuplets[i].chords.empty())
                  continue;
            const int tupletVoice = tuplets[i].chords.begin()->second->second.voice;
            if (tupletVoice == newVoice) {
                  if (changedTuplets.find(i) != changedTuplets.end())
                        return false;
                  }
            }
      return true;
      }

void changeTupletVoice(TupletInfo &tuplet,
                       int voice)
      {
      for (auto &chord: tuplet.chords)
            chord.second->second.voice = voice;
      }

void exchangeVoicesOfTuplets(std::vector<TupletInfo> &tuplets,
                             int oldVoice,
                             int newVoice)
      {
      for (int i = 0; i != (int)tuplets.size(); ++i) {
            if (tuplets[i].chords.empty())
                  continue;
            const int tupletVoice = tuplets[i].chords.begin()->second->second.voice;
            if (tupletVoice == newVoice)
                  changeTupletVoice(tuplets[i], oldVoice);
            }
      }

void exchangeVoicesOfNonTuplets(std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &nonTuplets,
                                int oldVoice,
                                int newVoice)
      {
      for (auto &nonTuplet: nonTuplets) {
            if (nonTuplet->second.voice == oldVoice)
                  nonTuplet->second.voice = newVoice;
            }
      }

void assignVoices(std::multimap<ReducedFraction, MidiChord> &chords,
                  std::vector<MidiTuplet::TupletInfo> &tuplets,
                  const std::multimap<ReducedFraction, MidiChord>::iterator &startBarChordIt,
                  const std::multimap<ReducedFraction, MidiChord>::iterator &endBarChordIt,
                  const ReducedFraction &endBarTick,
                  const ReducedFraction &barLen)
      {
      sortNotesByPitch(startBarChordIt, endBarChordIt);
      sortTupletsByAveragePitch(tuplets);
      splitFirstTupletChords(tuplets, chords);

      separateTupletVoices(tuplets, startBarChordIt, endBarChordIt, chords);
      const auto regularRaster = Quantize::findRegularQuantRaster(
                                       startBarChordIt, endBarChordIt, endBarTick);
      auto nonTuplets = findNonTupletChords(tuplets, startBarChordIt, endBarChordIt);
      setNonTupletVoices(tuplets, nonTuplets, regularRaster);
      std::set<int> notMovableVoices;
      mergeVoices(tuplets, nonTuplets, notMovableVoices);
      shrinkVoices(startBarChordIt, endBarChordIt, notMovableVoices);
      adjustNonTupletVoices(nonTuplets, tuplets);
      }

} // namespace MidiVoice
} // namespace Ms
