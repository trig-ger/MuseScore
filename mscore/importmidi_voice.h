#ifndef IMPORTMIDI_VOICE_H
#define IMPORTMIDI_VOICE_H


namespace Ms {

class ReducedFraction;
class MidiChord;

namespace MidiTuplet {
struct TupletInfo;
}

namespace MidiVoice {

void assignVoices(std::multimap<ReducedFraction, MidiChord> &chords,
                  std::vector<MidiTuplet::TupletInfo> &tuplets,
                  const std::multimap<ReducedFraction, MidiChord>::iterator &startBarChordIt,
                  const std::multimap<ReducedFraction, MidiChord>::iterator &endBarChordIt,
                  const ReducedFraction &endBarTick,
                  const ReducedFraction &barLen);

} // namespace MidiVoice
} // namespace Ms


#endif // IMPORTMIDI_VOICE_H
