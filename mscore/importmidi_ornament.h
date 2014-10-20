#ifndef IMPORTMIDI_ORNAMENT_H
#define IMPORTMIDI_ORNAMENT_H


namespace Ms {

class Staff;
class MTrack;

namespace MidiOrnament {

void detectTremolo(Staff *staff, int indexOfOperation, bool isDrumTrack);

} // namespace MidiOrnament
} // namespace Ms


#endif // IMPORTMIDI_ORNAMENT_H
