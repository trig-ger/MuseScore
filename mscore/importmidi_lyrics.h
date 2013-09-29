#ifndef IMPORTMIDI_LYRICS_H
#define IMPORTMIDI_LYRICS_H


namespace Ms {

class MidiFile;
class MTrack;

namespace MidiLyrics {

void assignLyricsToTracks(QList<MTrack> &tracks);
void setLyricsFromOperations(const QList<MTrack> &tracks);
void setInitialLyricsFromMidiData(const QList<MTrack> &tracks);
void extractLyricsToMidiData(const MidiFile *mf);
QList<std::string> makeLyricsListForUI(unsigned int symbolLimit = 20);

} // namespace MidiLyrics
} // namespace Ms


#endif // IMPORTMIDI_LYRICS_H
