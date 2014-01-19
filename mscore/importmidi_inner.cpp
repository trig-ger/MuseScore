#include "importmidi_inner.h"
#include "preferences.h"


namespace Ms {
namespace MidiCharset {

QString convertToCharset(const std::string &text)
      {
                  // charset for the current MIDI file
      QString charset = preferences.midiImportOperations.charset();
      auto *codec = QTextCodec::codecForName(charset.toLatin1());
      if (codec)
            return codec->toUnicode(text.c_str());
      else
            return QString::fromStdString(text);
      }

QString defaultCharset()
      {
      return "UTF-8";
      }

std::string fromUchar(const uchar *text)
      {
      return reinterpret_cast<const char*>(text);
      }

} // namespace MidiCharset

namespace MidiTempo {

ReducedFraction time2Tick(double time, double ticksPerSec)
      {
      return ReducedFraction::fromTicks(int(ticksPerSec * time));
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

} // namespace MidiTempo
} // namespace Ms
