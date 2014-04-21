#ifndef IMPORTMIDI_QUANT_H
#define IMPORTMIDI_QUANT_H

#include "importmidi_operation.h"


namespace Ms {

class MidiChord;
class TimeSigMap;
class ReducedFraction;

namespace Quantize {

ReducedFraction userQuantNoteToFraction(MidiOperation::QuantValue quantNote);

ReducedFraction quantForLen(
            const ReducedFraction &basicQuant,
            const ReducedFraction &noteLen,
            const ReducedFraction &tupletRatio);

ReducedFraction findQuantForRange(
            const std::multimap<ReducedFraction, MidiChord>::const_iterator &beg,
            const std::multimap<ReducedFraction, MidiChord>::const_iterator &end,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio);

ReducedFraction findQuantizedChordOnTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio,
            const ReducedFraction &rangeStart);

ReducedFraction findQuantizedChordOnTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant);

ReducedFraction findQuantizedNoteOffTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &offTime,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio,
            const ReducedFraction &rangeStart);

ReducedFraction findQuantizedNoteOffTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &offTime,
            const ReducedFraction &basicQuant);

ReducedFraction findMinQuantizedOnTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio,
            const ReducedFraction &rangeStart);

ReducedFraction findMinQuantizedOnTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant);

ReducedFraction findMaxQuantizedOffTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio,
            const ReducedFraction &rangeStart);

ReducedFraction findMaxQuantizedOffTime(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant);

ReducedFraction findOnTimeQuantError(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio,
            const ReducedFraction &rangeStart);

ReducedFraction findOnTimeQuantError(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &basicQuant);

ReducedFraction findOffTimeQuantError(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &offTime,
            const ReducedFraction &basicQuant,
            const ReducedFraction &tupletRatio,
            const ReducedFraction &rangeStart);

ReducedFraction findOffTimeQuantError(
            const std::pair<const ReducedFraction, MidiChord> &chord,
            const ReducedFraction &offTime,
            const ReducedFraction &basicQuant);

void quantizeChords(
            std::multimap<ReducedFraction, MidiChord> &chords,
            const TimeSigMap *sigmap,
            const ReducedFraction &basicQuant);

} // namespace Quantize
} // namespace Ms


#endif // IMPORTMIDI_QUANT_H
