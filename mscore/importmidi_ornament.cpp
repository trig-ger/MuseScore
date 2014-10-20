#include "importmidi_ornament.h"
#include "importmidi_meter.h"
#include "libmscore/staff.h"
#include "libmscore/score.h"
#include "libmscore/chordrest.h"
#include "libmscore/chord.h"


namespace Ms {

void MidiOrnament::detectTremolo(Staff *staff, int indexOfOperation, bool isDrumTrack)
      {
      if (isDrumTrack)
            return;

      const int strack = staff->idx() * VOICES;
      std::vector<Segment *> tremoloSegments;
      int tremoloPitch = -1;

      for (int voice = 0; voice < VOICES; ++voice) {
            for (Segment *seg = staff->score()->firstSegment(Segment::Type::ChordRest); seg;
                 seg = seg->next1(Segment::Type::ChordRest)) {

                  ChordRest *cr = static_cast<ChordRest *>(seg->element(strack + voice));
                  if (!cr || cr->type() != Element::Type::CHORD)
                        continue;
                  if (cr->tuplet())
                        continue;

                  const ReducedFraction duration(cr->duration());
                  if (duration > ReducedFraction(1, 8) || !Meter::isSimpleNoteDuration(duration))
                        continue;
                  Chord *chord = static_cast<Chord *>(cr);
                  const auto &notes = chord->notes();
                  if (notes.size() != 1)
                        continue;
                  const Note *note = notes.front();
                  if (note->tieFor() || note->tieBack())
                        continue;

                  if (tremoloPitch == -1)
                        tremoloPitch = note->pitch();
                  if (note->pitch() == tremoloPitch) {
                        tremoloSegments.push_back(seg);
                        }
                  else {
                        const Fraction totalDuration = duration * tremoloSegments.size();
                        if (totalDuration <= breve && isPowerOfTwo(tremoloSegments.size())) {
                              insertTremolo(totalDuration);
                              }
                        tremoloSegments.clear();
                        }
                  }
            }
      if (totalDuration <= breve && isPowerOfTwo(tremoloSegments.size())) {
            // insert pending tremolo
            insertTremolo(totalDuration);
            }
      }

} // namespace Ms