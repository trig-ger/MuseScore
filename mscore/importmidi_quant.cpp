#include "importmidi_quant.h"
#include "libmscore/sig.h"
#include "libmscore/utils.h"
#include "libmscore/fraction.h"
#include "libmscore/mscore.h"
#include "preferences.h"
#include "importmidi_chord.h"
#include "importmidi_meter.h"


namespace Ms {

extern Preferences preferences;

namespace Quantize {

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


int shortestNoteInBar(const std::multimap<int, MidiChord> &chords,
                      const std::multimap<int, MidiChord>::const_iterator &start,
                      int endBarTick)
      {
      int division = MScore::division;
      int minDuration = division;

      // find shortest note in measure
      //
      for (auto i = start; i != chords.end(); ++i) {
            if (i->first >= endBarTick)
                  break;
            for (const auto &note: i->second.notes)
                  minDuration = qMin(minDuration, note.len);
            }
      //
      // determine suitable quantization value based
      // on shortest note in measure
      //
      int div = division;
      if (minDuration <= division / 16)        // minimum duration is 1/64
            div = division / 16;
      else if (minDuration <= division / 8)
            div = division / 8;
      else if (minDuration <= division / 4)
            div = division / 4;
      else if (minDuration <= division / 2)
            div = division / 2;
      else if (minDuration <= division)
            div = division;
      else if (minDuration <= division * 2)
            div = division * 2;
      else if (minDuration <= division * 4)
            div = division * 4;
      else if (minDuration <= division * 8)
            div = division * 8;
      if (div == (division / 16))
            minDuration = div;
      else
            minDuration = quantizeLen(minDuration, div >> 1);    //closest

      return minDuration;
      }

int userQuantNoteToTicks(MidiOperation::QuantValue quantNote)
      {
      int division = MScore::division;
      int userQuantValue = preferences.shortestNote;
      // specified quantization value
      switch (quantNote) {
            case MidiOperation::QuantValue::N_4:
                  userQuantValue = division;
                  break;
//            case MidiOperation::QuantValue::N_4_triplet:
//                  userQuantValue = division * 2 / 3;
//                  break;
            case MidiOperation::QuantValue::N_8:
                  userQuantValue = division / 2;
                  break;
//            case MidiOperation::QuantValue::N_8_triplet:
//                  userQuantValue = division / 3;
//                  break;
            case MidiOperation::QuantValue::N_16:
                  userQuantValue = division / 4;
                  break;
//            case MidiOperation::QuantValue::N_16_triplet:
//                  userQuantValue = division / 6;
//                  break;
            case MidiOperation::QuantValue::N_32:
                  userQuantValue = division / 8;
                  break;
//            case MidiOperation::QuantValue::N_32_triplet:
//                  userQuantValue = division / 12;
//                  break;
            case MidiOperation::QuantValue::N_64:
                  userQuantValue = division / 16;
                  break;
            case MidiOperation::QuantValue::FROM_PREFERENCES:
            default:
                  userQuantValue = preferences.shortestNote;
                  break;
            }

      return userQuantValue;
      }

int findQuantRaster(const std::multimap<int, MidiChord> &chords,
                    const std::multimap<int, MidiChord>::const_iterator &startChordIter,
                    int endBarTick)
      {
      int raster;
      auto operations = preferences.midiImportOperations.currentTrackOperations();

      // find raster value for quantization
      if (operations.quantize.value == MidiOperation::QuantValue::SHORTEST_IN_BAR)
            raster = shortestNoteInBar(chords, startChordIter, endBarTick);
      else {
            int userQuantValue = userQuantNoteToTicks(operations.quantize.value);
            // if user value larger than the smallest note in bar
            // then use the smallest note to keep faster events
            if (operations.quantize.reduceToShorterNotesInBar) {
                  raster = shortestNoteInBar(chords, startChordIter, endBarTick);
                  raster = qMin(userQuantValue, raster);
                  }
            else
                  raster = userQuantValue;
            }
      return raster;
      }

void doGridQuantizationOfBar(const std::multimap<int, MidiChord> &chords,
                             std::multimap<int, MidiChord> &quantizedChords,
                             const std::multimap<int, MidiChord>::const_iterator &startChordIter,
                             int raster,
                             int endBarTick)
      {
      int raster2 = raster >> 1;
      for (auto i = startChordIter; i != chords.end(); ++i) {
            if (i->first >= endBarTick)
                  break;
            auto chord = i->second;
            chord.onTime = ((chord.onTime + raster2) / raster) * raster;
            for (auto &note: chord.notes) {
                  note.onTime = chord.onTime;
                  note.len  = quantizeLen(note.len, raster);
                  }
            quantizedChords.insert({chord.onTime, chord});
            }
      }

const std::multimap<int, MidiChord>::const_iterator
findFirstChordInBar(const std::multimap<int, MidiChord> &chords, int startBarTick, int endBarTick)
      {
      auto i = chords.begin();
      for (; i != chords.end(); ++i) {
            if (i->first >= startBarTick) {
                  if (i->first >= endBarTick)
                        return chords.end();
                  break;
                  }
            }
      return i;
      }

void quantizeChordsOfBar(const std::multimap<int, MidiChord> &chords,
                         std::multimap<int, MidiChord> &quantizedChords,
                         int startBarTick,
                         int endBarTick)
      {
      auto startChordIter = findFirstChordInBar(chords, startBarTick, endBarTick);
      if (startChordIter == chords.end()) // if no chords found in this bar
            return;
      int raster = findQuantRaster(chords, startChordIter, endBarTick);
      doGridQuantizationOfBar(chords, quantizedChords, startChordIter, raster, endBarTick);
      }

void applyGridQuant(std::multimap<int, MidiChord> &chords,
                    const TimeSigMap* sigmap,
                    int lastTick)
      {
      std::multimap<int, MidiChord> quantizedChords;
      int startBarTick = 0;
      for (int i = 1;; ++i) { // iterate over all measures by indexes
            int endBarTick = sigmap->bar2tick(i, 0);
            quantizeChordsOfBar(chords, quantizedChords, startBarTick, endBarTick);
            if (endBarTick > lastTick)
                  break;
            startBarTick = endBarTick;
            }
      chords = quantizedChords;
      }

} // namespace Quantize
} // namespace Ms
