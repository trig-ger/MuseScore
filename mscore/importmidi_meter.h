#ifndef IMPORTMIDI_METER_H
#define IMPORTMIDI_METER_H

#include <vector>


namespace Ms {

class ReducedFraction;
class TDuration;
class TimeSigMap;

namespace MidiTuplet {
struct TupletData;
}

namespace Meter {

enum class DurationType
      {
      NOTE,
      REST
      };

bool isSimple(const ReducedFraction &barFraction);
bool isCompound(const ReducedFraction &barFraction);
bool isComplex(const ReducedFraction &barFraction);
bool isDuple(const ReducedFraction &barFraction);
bool isTriple(const ReducedFraction &barFraction);
bool isQuadruple(const ReducedFraction &barFraction);
bool isQuintuple(const ReducedFraction &barFraction);
bool isSeptuple(const ReducedFraction &barFraction);

ReducedFraction beatLength(const ReducedFraction &barFraction);

struct DivisionInfo;

DivisionInfo metricDivisionsOfBar(const ReducedFraction &barFraction);
DivisionInfo metricDivisionsOfTuplet(const MidiTuplet::TupletData &tuplet,
                                     int tupletStartLevel);
            // levels start from the beginning of the bar; end of the bar is not included
std::vector<int> metricLevelsOfBar(const ReducedFraction &barFraction,
                                   const std::vector<DivisionInfo> &divsInfo,
                                   const ReducedFraction &minDuration);

bool isSimpleNoteDuration(const ReducedFraction &duration);   // quarter, half, eighth, 16th ...
bool isSingleDottedDuration(const ReducedFraction &duration);

            // division lengths of bar, each can be a tuplet length
std::vector<ReducedFraction> divisionsOfBarForTuplets(const ReducedFraction &barFraction);

            // metric structure of bar
std::vector<DivisionInfo> findBarDivisionInfo(
            const ReducedFraction &barFraction,
            const std::vector<MidiTuplet::TupletData> &tupletsInBar);

std::vector<DivisionInfo> findBarDivisionInfo(
            const ReducedFraction &someTimeInBar,
            const TimeSigMap *sigmap,
            int voice,
            const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets);

int levelOfTick(const ReducedFraction &tick, const std::vector<DivisionInfo> &divsInfo);

struct MaxLevel;
MaxLevel findMaxLevelBetween(const ReducedFraction &startTickInBar,
                             const ReducedFraction &endTickInBar,
                             const std::vector<DivisionInfo> &divsInfo);

            // duration and all tuplets should belong to the same voice
// nested tuplets are not allowed
QList<std::pair<ReducedFraction, TDuration> >
toDurationList(const ReducedFraction &startTickInBar,
               const ReducedFraction &endTickInBar,
               const ReducedFraction &barFraction,
               const std::vector<MidiTuplet::TupletData> &tupletsInBar,
               DurationType durationType,
               bool useDots);

} // namespace Meter
} // namespace Ms


#endif // IMPORTMIDI_METER_H
