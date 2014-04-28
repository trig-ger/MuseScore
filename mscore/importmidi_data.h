#ifndef IMPORTMIDI_DATA_H
#define IMPORTMIDI_DATA_H

#include "midi/midifile.h"
#include "importmidi_fraction.h"


namespace Ms {

namespace MidiCharset {
      QString defaultCharset();
}

struct TrackData;

class MidiData
      {
   public:
      void setTracksData(const QString &fileName, const QList<TrackData> &tracksData);
      void saveHHeaderState(const QString &fileName, const QByteArray &headerData);
      void saveVHeaderState(const QString &fileName, const QByteArray &headerData);
      void excludeFile(const QString &fileName);
      QList<TrackData> tracksData(const QString &fileName) const;
      QByteArray HHeaderData(const QString &fileName) const;
      QByteArray VHeaderData(const QString &fileName) const;
      int selectedRow(const QString &fileName) const;
      void setSelectedRow(const QString &fileName, int row);
      void setMidiFile(const QString &fileName, const MidiFile &midiFile);
      const MidiFile *midiFile(const QString &fileName) const;
                  // lyrics
      void addTrackLyrics(const QString &fileName,
                          const std::multimap<ReducedFraction, std::string> &trackLyrics);
      const QList<std::multimap<ReducedFraction, std::string> > *
            getLyrics(const QString &fileName);
      QString charset(const QString &fileName) const;
      void setCharset(const QString &fileName, const QString &charset);
                  // human performance: is MIDI unaligned
      bool isHumanPerformance(const QString &fileName) const;
      void setHumanPerformance(const QString &fileName, bool value);

   private:
      struct MidiDataStore
            {
            QByteArray HHeaderData;
            QByteArray VHeaderData;
            QList<TrackData> tracksData;
                        // tracks of <tick, lyric fragment> from karaoke files
            QList<std::multimap<ReducedFraction, std::string>> lyricTracks;
            int selectedRow = 0;
            MidiFile midiFile;
            QString charset = MidiCharset::defaultCharset();
            bool isHumanPerformance = false;
            };
      QMap<QString, MidiDataStore> data;    // <file name, tracks data>
      };

} // namespace Ms


#endif // IMPORTMIDI_DATA_H
