#include "importmidi_quant.h"
#include "libmscore/sig.h"
#include "importmidi_fraction.h"
#include "libmscore/mscore.h"
#include "preferences.h"
#include "importmidi_chord.h"
#include "importmidi_meter.h"
#include "importmidi_tuplet.h"
#include "thirdparty/beatroot/BeatTracker.h"

#include <set>
#include <functional>


namespace Ms {

extern Preferences preferences;

namespace Quantize {

ReducedFraction shortestNoteInBar(const std::multimap<ReducedFraction, MidiChord>::const_iterator &startBarChordIt,
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
                  // determine suitable quantization value based
                  // on shortest note in measure
      auto div = division;
      for (ReducedFraction duration(1, 16); duration <= ReducedFraction(8, 1); duration *= 2) {
                        // minimum duration is 1/64
            if (minDuration <= division * duration) {
                  div = division * duration;
                  break;
                  }
            }
      if (div == (division / 16))
            minDuration = div;
      else
            minDuration = quantizeValue(minDuration, div / 2);    //closest

      return minDuration;
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
            const auto shortest = shortestNoteInBar(startBarChordIt, endChordIt, endBarTick);
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

            for (MidiNote &note: chord.notes) {
                  auto offTime = chordEvent.first + note.len;
                  raster = findQuantRaster(offTime, chord.voice, tupletEvents, chords, sigmap);
                  if (Meter::isSimpleNoteDuration(raster))    // offTime is not inside tuplet
                        raster = reduceRasterIfDottedNote(note.len, raster);
                  offTime = quantizeValue(offTime, raster);
                  note.len = offTime - onTime;
                  }
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

// tempo in beats per second

double findBasicTempo(const std::multimap<int, MTrack> &tracks)
      {
      for (const auto &track: tracks) {
            for (const auto &ie : track.second.mtrack->events()) {
                  const MidiEvent &e = ie.second;
                  if (e.type() == ME_META && e.metaType() == META_TEMPO) {
                        const uchar* data = (uchar*)e.edata();
                        const unsigned tempo = data[2] + (data[1] << 8) + (data[0] << 16);
                        return 1000000.0 / double(tempo);
                        }
                  }
            }

      return 2;   // default beats per second = 120 beats per minute
      }

ReducedFraction time2Tick(double time, double ticksPerSec)
      {
      return ReducedFraction::fromTicks(int(ticksPerSec * time));
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
            beatSet.insert(time2Tick(beatTime, ticksPerSec));
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
            saliences.insert({time2Tick(e.time, ticksPerSec), e.salience});
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
                              const TimeSigMap *sigmap)
      {
      auto allChordsTrack = getTrackWithAllChords(tracks);
      MChord::collectChords(allChordsTrack);
      const MTrack &track = allChordsTrack.begin()->second;
      const auto &allChords = track.chords;
      if (allChords.empty())
            return;

      const double ticksPerSec = findBasicTempo(tracks) * MScore::division;

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









// max allowed tempo scale mismatch between midi events and current timeline
// after adaptive quantization
const int SCORE_LENGTH_SCALE = 1;


// allTicks - length of the whole score in ticks
std::vector<int> metricLevelsOfScore(const TimeSigMap *sigmap, int allTicks, int minDuration)
      {
      std::vector<int> levels;
      // here we use one timesig for all score
      Fraction timesig(4, 4); // default
      if (!sigmap->empty())
            timesig = sigmap->begin()->second.timesig();
      auto levelsOfBar = Meter::metricLevelsOfBar(timesig, minDuration);
      // add the level of start point of the first bar in score
      levels.push_back(levelsOfBar.front());

      int ticksPerBar = timesig.ticks();
      int allScoreTicks = SCORE_LENGTH_SCALE * allTicks;
      int barCount = allScoreTicks / ticksPerBar;
      for (int i = 0; i != barCount; ++i) { // i is just a counter here
            // +1 because end of the bar is the start of the next bar
            levels.insert(levels.end(), levelsOfBar.begin() + 1, levelsOfBar.end());
            }
      return levels;
      }
/*
std::vector<int> metricLevelsOfScore(const Score *score, int minDuration)
      {
      std::vector<int> levels;
      int lastTick = score->lastMeasure()->endTick();

      for (auto is = score->sigmap()->begin(); is != score->sigmap()->end(); ++is) {
            auto next = is;
            ++next;
            int curTimesigTick = is->first;
            int nextTimesigTick = (next == score->sigmap()->end()) ? lastTick : next->first;

            Fraction timesig = is->second.timesig();
            auto levelsOfBar = Meter::metricLevelsOfBar(timesig, minDuration);
            // add the level of start point of the first bar in score
            if (levels.empty())
                  levels.push_back(levelsOfBar.front());

            int ticksPerBar = timesig.ticks();
            int barsWithThisTimesig = (nextTimesigTick - curTimesigTick) / ticksPerBar;
            for (int i = 0; i != barsWithThisTimesig; ++i) { // i is just a counter here
                  // +1 because end of the bar is the start of the next bar
                  levels.insert(levels.end(), levelsOfBar.begin() + 1, levelsOfBar.end());
                  }
            }
      return levels;
      }
*/

int averageChordVelocity(const QList<MidiNote> &notes)
      {
      int sum = 0;
      for (const auto &note: notes)
            sum += note.velo;
      return sum / notes.size();
      }

int maxChordVelocity(const QList<MidiNote> &notes)
      {
      if (notes.empty())
            return 0;
      int maxVelo = notes.front().velo;
      for (const auto &note: notes) {
            if (note.velo > maxVelo)
                  maxVelo = note.velo;
            }
      return maxVelo;
      }

void metricLevelsToWeights(std::vector<int> &levels, const std::multimap<int, MidiChord> &chords)
      {
                  // here chord weights = chord velocities as in MIDI file
                  // adapt levels to be in the range of [min weight .. max weight] of chords
                  // because now levels are 0, -1, -2 ... - far from chord weights
      if (chords.empty())
            return;
      int maxChordVel = maxChordVelocity(chords.begin()->second.notes);
      int minChordVel = maxChordVel;

      for (const auto &chordEvent: chords) {
            const auto &chord = chordEvent.second;
            int chordVelocity = maxChordVelocity(chord.notes);

            if (chordVelocity > maxChordVel)
                  maxChordVel = chordVelocity;
            if (chordVelocity < minChordVel)
                  minChordVel = chordVelocity;
            }
      int minLevel = *std::min_element(levels.begin(), levels.end());
      int maxLevel = *std::max_element(levels.begin(), levels.end());

      double scale = (maxChordVel - minChordVel) * 1.0 / (maxLevel - minLevel);
      for (auto &level: levels)
            level = minChordVel + std::round(scale * (level - minLevel));
      }


template <typename T>
class StepStore
      {
   public:
      StepStore(int stepCount, int iCount, int jCount)
            : store(stepCount * iCount * jCount, std::numeric_limits<T>::max())
            , iCount(iCount), jCount(jCount)
            {}
      T& operator()(int step, int i, int j)
            {
            return store[step * iCount * jCount + i * jCount + j];
            }
   private:
      std::vector<T> store;
      int iCount;
      int jCount;
      };

// based on Jeff Lieberman's algorithm of intelligent quantization
void adaptiveQuant(std::multimap<int, MidiChord> &chords,
                   const std::vector<int> &templateWeights,
                   int minDuration)
      {
      if (chords.empty())
            return;
      int chordCount = chords.size();
      int templatePointCount = templateWeights.size();

      const int beta = 100;
      const int gamma = 100;
      const int delta = 1000;

      // -- not now -- remember only 2 step history for costs
      StepStore<double> totalCosts(chordCount, templatePointCount, templatePointCount);
      // remember all history for chord positions
      StepStore<int> prevBestChoices(chordCount, templatePointCount, templatePointCount);

      std::vector<int> chordTimes;
      std::vector<int> chordWeights;
      std::vector<int> templateTimes;

      for (const auto &chordEvent: chords) {
            const auto &chord = chordEvent.second;
            chordTimes.push_back(chord.onTime);
            int chordVelocity = maxChordVelocity(chord.notes);
            chordWeights.push_back(chordVelocity);
            }

      int firstTimeShift = chords.begin()->first;
      for (int i = 0; i != templatePointCount; ++i)
            templateTimes.push_back(firstTimeShift + i * minDuration);


      // switchers for array for current and previous steps
//      int now = 0;
//      int prev = 1;

      int step = 0;                 // which step we are trying to fit
      int j = 0;
      for (int i = step; i != templatePointCount; ++i) {       // current choice for this step in template
            double timeShift = chordTimes[step] - templateTimes[i];
            double weightShift = chordWeights[step] - templateWeights[i];
            double cost = beta * (timeShift * timeShift)
                        + gamma * (weightShift * weightShift);
            totalCosts(step, i, j) = cost;
            }

//      now = 1 - now;
//      prev = 1 - prev;

      step = 1;                 // which step we are trying to fit
      int k = 0;
      for (int i = step; i != templatePointCount; ++i) {      // current choice for this step in template, can't be earlier than it's number
            for (int j = step - 1; j != i; ++j) {        // current choice for previous step in template
                  double timeShift = chordTimes[step] - templateTimes[i];
                  double weightShift = chordWeights[step] - templateWeights[i];
                  double cost = beta * (timeShift * timeShift)
                              + gamma * (weightShift * weightShift)
                              + totalCosts(step - 1, j, k);
                  int minK = 0;  // also negligable
                  // total cost is minimum - this is total cost so far, for this step, if i and j are the best choice:
                  totalCosts(step, i, j) = cost;
                  // now the k that yielded the minimum total cost is the best previous choice:
                  prevBestChoices(step, i, j) = minK;
                  }
            }



      std::vector<double> costs(templatePointCount - 2);

      for (int step = 2; step != chordCount; ++step) {                 // which step we are trying to fit
//            now = 1 - now;
//            prev = 1 - prev;
            for (int i = step; i != templatePointCount; ++i) {        // current choice for this step in template, can't be earlier than it's number
                  for (int j = step - 1; j != i; ++j) {       // current choice for previous step in template
                        // don't need to initialize cost, because we always write it to a bigger value, i think
                        for (int k = step - 2; k != j; ++k) {        // current choice for second previous step in template
                              // generate costs: [do not need to save] for each choice of i,j. then choose best k given those
                              double oldTempoScale = (templateTimes[j] - templateTimes[k])
                                          * 1.0 / (chordTimes[step - 1] - chordTimes[step - 2]);
                              double newTempoScale = (templateTimes[i] - templateTimes[j])
                                          * 1.0 / (chordTimes[step] - chordTimes[step - 1]);
                              double tempoScaleChange = newTempoScale - oldTempoScale;
                              double timeShift = chordTimes[step] - templateTimes[i];
                              double weightShift = chordWeights[step] - templateWeights[i];
                              costs[k] = beta * (timeShift * timeShift)
                                       + gamma * (weightShift * weightShift)
                                       + delta * (tempoScaleChange * tempoScaleChange)
                                       + totalCosts(step - 1, j, k);
                              }

                        // find the best k
                        double minCost = costs[step - 2];
                        int minK = step - 2;
                        for (int ii = step - 1; ii < j; ++ii) {
                              if (costs[ii] < minCost) {
                                    minCost = costs[ii];
                                    minK = ii;
                                    }
                              }

                        // total cost is minimum - this is total cost so far, for this step, if i and j are the best choice:
                        totalCosts(step, i, j) = minCost;
                        // now the k that yielded the minimum total cost is the best previous choice:
                        prevBestChoices(step, i, j) = minK;
                        }
                  }
            }


//      for (int step = 0; step != 2; ++step) {
//            qDebug() << "step " << step << "\n";
//            for (int i = 0; i != templatePointCount; ++i) {
//                  QString val;
//                  for (int j = 0; j != templatePointCount; ++j) {
//                        double num = totalCosts(step, i, j);
//                        if (num == std::numeric_limits<double>::max()) {
//                              val += "* ";
//                              }
//                        else {
//                              val += QString::number(num) + " ";
//                              }
//                        }
//                  qDebug() << val;
//                  }
//            }


      // try to find best two final choices
      int bestPrevChoice = 0;
      {
      double minCost = totalCosts(chordCount - 1, 0, 0);
      for (int j = 0; j != templatePointCount; ++j) {
            for (int i = 0; i != templatePointCount; ++i) {
                  double cost = totalCosts(chordCount - 1, i, j);
                  if (cost < minCost) {
                        minCost = cost;
                        bestPrevChoice = j;
                        }
                  }
            }
      }

      int bestNowChoice = 0;
      {
      double minCost = totalCosts(chordCount - 1, 0, bestPrevChoice);
      for (int i = 0; i != templatePointCount; ++i) {
            double cost = totalCosts(chordCount - 1, i, bestPrevChoice);
            if (cost < minCost) {
                  minCost = cost;
                  bestNowChoice = i;
                  }
            }
      }

      std::vector<int> bestChoices(chordCount);
      bestChoices[chordCount - 1] = bestNowChoice;
      bestChoices[chordCount - 2] = bestPrevChoice;

      // travel backwards, finding each previous best choice
      // pattern is prevBestChoices(step, i, j)
      // i -> step, j -> step - 1, k -> step - 2
      for (int step = chordCount - 3; step >= 0; --step)
            bestChoices[step] = prevBestChoices(step + 2, bestChoices[step + 2], bestChoices[step + 1]);

      // correct onTime value of every chord and note
      // correct duration for every chord and note by scaling
      int chordCounter = 0;
      double scale = 1.0;
      for (auto p = chords.begin(); p != chords.end(); ++p) {
            auto &chord = p->second;
            int newOnTime = templateTimes[bestChoices[chordCounter]];
            int oldOnTime = chord.onTime;

            int nextNewOnTime = 1;
            int nextOldOnTime = 2;
            auto next = p;
            ++next;
            if (next != chords.end()) {
                  nextNewOnTime = templateTimes[bestChoices[chordCounter + 1]];
                  nextOldOnTime = next->second.onTime;
                  scale = (nextNewOnTime - newOnTime) * 1.0 / (nextOldOnTime - oldOnTime);
                  }

            chord.onTime = newOnTime;
            // chord duration here is undefined because notes may have different durations
            // so don't scale chord duration here, scale note durations instead
            for (auto &note: chord.notes) {
                  note.onTime = newOnTime;
                  note.len = std::round(note.len * scale);
                  }
            ++chordCounter;
            }
      }

void applyAdaptiveQuant(std::multimap<int, MidiChord> &chords,
                        const TimeSigMap *sigmap,
                        int allTicks)
      {
      int minDuration = MScore::division / 4; // 1/16 note
      auto levels = metricLevelsOfScore(sigmap, allTicks, minDuration);
      metricLevelsToWeights(levels, chords);
      adaptiveQuant(chords, levels, minDuration);
      }


} // namespace Quantize
} // namespace Ms
