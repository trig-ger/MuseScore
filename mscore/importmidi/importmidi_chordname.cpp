#include "importmidi_chordname.h"
#include "importmidi_inner.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "libmscore/score.h"
#include "libmscore/staff.h"
#include "libmscore/measure.h"
#include "libmscore/harmony.h"
#include "midi/midifile.h"
#include "mscore/preferences.h"


// From XF Format Specifications V 2.01 (January 13, 1999, YAMAHA CORPORATION)

namespace Ms {
namespace MidiChordName {

int readFirstHalf(uchar byte)
      {
      return (byte >> 4) & 0xf;
      }

int readSecondHalf(uchar byte)
      {
      return byte & 0xf;
      }

QString readChordRoot(uchar byte)
      {
      static const std::vector<QString> inversions = {
            "bbb", "bb", "b", "", "#", "##", "###"
            };
      static const std::vector<QString> notes = {
            "", "C", "D", "E", "F", "G", "A", "B"
            };

      QString chordRoot;

      const size_t noteIndex = readSecondHalf(byte);
      if (noteIndex < notes.size())
            chordRoot += notes[noteIndex];

      const size_t inversionIndex = readFirstHalf(byte);
      if (inversionIndex < inversions.size())
            chordRoot += inversions[inversionIndex];

      return chordRoot;
      }

QString readChordType(uchar chordTypeIndex)
      {
      static const std::vector<QString> chordTypes = {
            "Maj"
          , "Maj6"
          , "Maj7"
          , "Maj7(#11)"
          , "Maj(9)"
          , "Maj7(9)"
          , "Maj6(9)"
          , "aug"
          , "min"
          , "min6"
          , "min7"
          , "min7b5"
          , "min(9)"
          , "min7(9)"
          , "min7(11)"
          , "minMaj7"
          , "minMaj7(9)"
          , "dim"
          , "dim7"
          , "7th"
          , "7sus4"
          , "7b5"
          , "7(9)"
          , "7(#11)"
          , "7(13)"
          , "7(b9)"
          , "7(b13)"
          , "7(#9)"
          , "Maj7aug"
          , "7aug"
          , "1+8"
          , "1+5"
          , "sus4"
          , "1+2+5"
          , "cc"
            };

      if (chordTypeIndex < chordTypes.size())
            return chordTypes[chordTypeIndex];
      return "";
      }

QString readChordName(const MidiEvent &e)
      {
      if (e.type() != ME_META || e.metaType() != META_SPECIFIC)
            return "";
      if (e.len() < 4)
            return "";

      const uchar *data = e.edata();
      if (data[0] != 0x43 || data[1] != 0x7B || data[2] != 0x01)
            return "";

      QString chordName;

      if (e.len() >= 4)
            chordName += readChordRoot(data[3]);
      if (e.len() >= 5)
            chordName += readChordType(data[4]);

      if (e.len() >= 6) {
            const QString chordRoot = readChordRoot(data[5]);
            if (!chordRoot.isEmpty()) {
                  QString chordType;
                  if (e.len() >= 7)
                        chordType = readChordType(data[6]);
                  if (chordRoot + chordType == chordName)
                        chordName += "/" + chordRoot;
                  else
                        chordName += "/" + chordRoot + chordType;
                  }
            }

      return chordName;
      }

QString findChordName(
            const QList<MidiNote> &notes,
            const std::multimap<ReducedFraction, QString> &chordNames)
      {
      for (const MidiNote &note: notes) {
            const auto range = chordNames.equal_range(note.origOnTime);
            if (range.second == range.first)
                  continue;
                        // found chord names (usually only one)
            QString chordName;
            for (auto it = range.first; it != range.second; ++it) {
                  if (it != range.first)
                        chordName += "\n" + it->second;
                  else
                        chordName += it->second;
                  }
            return chordName;
            }
      return "";
      }

void findChordNames(const std::multimap<int, MTrack> &tracks)
      {
      auto &data = *preferences.midiImportOperations.data();

      for (const auto &track: tracks) {
            for (const auto &event: track.second.mtrack->events()) {
                  const MidiEvent &e = event.second;
                  const QString chordName = readChordName(e);
                  if (!chordName.isEmpty()) {
                        const auto time = ReducedFraction::fromTicks(event.first);
                        data.chordNames.insert({time, chordName});
                        }
                  }
            }
      }

// all notes should be already placed to the score

void setChordNames(QList<MTrack> &tracks)
      {
      const auto &data = *preferences.midiImportOperations.data();
      if (data.chordNames.empty() || !data.trackOpers.showChordNames.value())
            return;

            // chords here can have on time very different from the original one
            // before quantization, so we look for original on times
            // that are stored in notes
      std::set<ReducedFraction> usedTimes;      // don't use one tick for chord name twice

      for (MTrack &track: tracks) {
            for (const auto &chord: track.chords) {
                  if (usedTimes.find(chord.first) != usedTimes.end())
                        continue;

                  const MidiChord &c = chord.second;
                  const QString chordName = findChordName(c.notes, data.chordNames);

                  if (chordName.isEmpty())
                        continue;

                  Staff *staff = track.staff;
                  Score *score = staff->score();
                  const ReducedFraction &onTime = chord.first;
                  usedTimes.insert(onTime);

                  Measure* measure = score->tick2measure(onTime.ticks());
                  Segment* seg = measure->getSegment(Segment::Type::ChordRest,
                                                     onTime.ticks());
                  const int t = staff->idx() * VOICES;

                  Harmony* h = new Harmony(score);
                  h->setHarmony(chordName);
                  h->setTrack(t);
                  seg->add(h);
                  }
            }
      }

// key profile score of all matches, see
//   D. Temperley - The Cognition of Basic Musical Structures (2001)
// pitchDistance - distance from tonic

double keyProfileScore(int pitchDistance)
      {
      static const std::vector<double> scores = {
         //  C   C#    D   Eb    E    F   F#    G   Ab    A   Bb    B
            5.0, 2.0, 3.5, 2.0, 4.5, 4.0, 2.0, 4.5, 2.0, 3.5, 1.5, 4.0
      };

      Q_ASSERT(pitchDistance >= 0 && pitchDistance < (int)scores.size());

      return scores[pitchDistance];
      }


bool isLess(const TemplateMatch &first, const TemplateMatch &second)
      {
                  // number of matched template elements
      if (allTemplateElementsMatch(first) && !allTemplateElementsMatch(second))
            return true;
      if (!allTemplateElementsMatch(first) && allTemplateElementsMatch(second))
            return false;
      if (allTemplateElementsMatch(first) && allTemplateElementsMatch(second)) {
                        // number of notes that matched tonic
            if (tonicMatchCount(first) != tonicMatchCount(second))
                  return tonicMatchCount(first) > tonicMatchCount(second);
                        // average number of notes per template element
            return (averageTemplateElementMatches(first) > averageTemplateElementMatches(second));
            }

      // the case !allTemplateElementsMatch(first) && !allTemplateElementsMatch(second)
                  // fraction of matched template elements of the total number of elements
      if (matchElementFraction(first) != matchElementFraction(second))
            return matchElementFraction(first) > matchElementFraction(second);
                  // number of notes that matched tonic
      if (tonicMatchCount(first) != tonicMatchCount(second))
            return tonicMatchCount(first) > tonicMatchCount(second);
                  // average number of notes per template element
      if (averageTemplateElementMatches(first) != averageTemplateElementMatches(second))
            return (averageTemplateElementMatches(first) > averageTemplateElementMatches(second));

                  // number of notes that do not match any template element
      if (notMatchedNotes(first) != notMatchedNotes(second))
            return notMatchedNotes(first) < notMatchedNotes(second);
                  // number of notes that do not match any template element
                  // and belong to the template tonality scale
      if (notMatchedScaleNotes(first) != notMatchedScaleNotes(second))
            return notMatchedScaleNotes(first) > notMatchedScaleNotes(second);
                  // approximate popularity of use
      return popularity(first) > popularity(second);
      }


class TemplateMatch
      {
   public:
      TemplateMatch(const ChordTemplate &templ, const QList<MidiNote> &notes)
            {

            }

      bool operator<(const TemplateMatch &other) const
            {

            }
   private:

      size_t allTemplateElementsMatch_;
      };

class ChordTemplate
      {
   public:
      ChordTemplate(const QString &name, const std::set<int> &pitches)
            : name_(name)
            , pitches_(pitches)
            {
            }

      QString name() const { return name_; }
      const std::set<int>& pitches() const { return pitches_; }

      bool hasPitch(int pitch) const
            {
            return pitches_.find(pitch % 12) != pitches_.end();
            }
   private:
      QString name_;
      std::set<int> pitches_;
            // approximate popularity of use in sample corpus
            // according to Pardo, B., & Birmingham, W. P. (2002).
            // Algorithms for Chordal Analysis. Computer Music Journal, Vol. 26, p. 27–49
      double popularity_;
      };

TemplateMatch findTemplateMatch(const MidiChord &chord, const ChordTemplate &templ)
      {
      TemplateMatch match;

      for (const MidiNote &note: chord.notes) {
            if (templ.hasPitch(note.pitch))
                  ++match.matched;
            }

      return match;
      }


struct TemplateScore
      {
      size_t templateIndex;
      double score;
      };

class ChordData
      {
   public:
      ChordData(const std::multimap<ReducedFraction, MidiChord>::const_iterator &chordIter)
            : chordIter_(chordIter)
            {
            findSuitableTemplates();
            }

   private:
      void findSuitableTemplates()
            {
            const auto &notes = chordIter_->second.notes;

            }

      std::multimap<ReducedFraction, MidiChord>::const_iterator chordIter_;
      std::vector<TemplateScore> templates_;
      };

class ChordNameSegment
      {
   public:
      ChordNameSegment(size_t startDataIndex, size_t endDataIndex)
            : startDataIndex_(startDataIndex), endDataIndex_(endDataIndex)
      {}

      size_t startDataIndex() const { return startDataIndex_; }
      size_t endDataIndex() const { return endDataIndex_; }

   private:
      size_t startDataIndex_;
      size_t endDataIndex_;
      };


void ff(const std::multimap<ReducedFraction, MidiChord> &allChords)
      {
      if (allChords.empty())
            return;

      std::vector<ChordData> chordsData;
      std::list<ChordNameSegment> segments;

      for (auto it = allChords.begin(); it != allChords.end(); ++it) {
            chordsData.push_back(ChordData(it));
            if (it->second.notes.size() >= 3)
                  segments.push_back(ChordNameSegment(chordsData.size() - 1, chordsData.size()));
            }

      for (auto it = segments.begin(); it != segments.end(); ++it) {
            const auto nextSeg = std::next(it);

            // if between segments there are chords that do not belong to any segment,
            //  put these chords into a new segment
            if (it->endDataIndex() != nextSeg->startDataIndex()) {
                  it = segments.insert(nextSeg, ChordNameSegment(it->endDataIndex(),
                                                                 nextSeg->startDataIndex()));
                  }
            }
      }






/*

class ChordNameSegment
      {
   public:
      ChordNameSegment(const std::multimap<ReducedFraction, MidiChord>::const_iterator &start,
                       const std::multimap<ReducedFraction, MidiChord>::const_iterator &end)
            : start_(start)
            , end_(end)
            {}

      bool operator<(const ChordNameSegment &other) const
            {
            return start_->first < other.start_->first;
            }
      std::multimap<ReducedFraction, MidiChord>::const_iterator start() const { return start_; }
      std::multimap<ReducedFraction, MidiChord>::const_iterator end() const { return end_; }

   private:
      std::multimap<ReducedFraction, MidiChord>::const_iterator start_;
      std::multimap<ReducedFraction, MidiChord>::const_iterator end_;
      };

class ChordName
      {
   public:
      enum class ChordType
            {
            MAJOR_TRIAD,
            MINOR_TRIAD,
            DIMINISHED_TRIAD
            };

      ChordName(int tonic, ChordType chordType) : tonic_(tonic), type_(chordType) {}

   private:
      int tonic_;        // pitch class number: 0-11
      ChordType type_;
      };

std::set<int> chordTypeToTemplate(ChordName::ChordType chordType)
      {
      switch (chordType) {
            case ChordName::ChordType::MAJOR_TRIAD:
                  return std::set<int>{0, 4, 7};
                  break;
            case ChordName::ChordType::MINOR_TRIAD:
                  return std::set<int>{0, 3, 7};
                  break;
            case ChordName::ChordType::DIMINISHED_TRIAD:
                  return std::set<int>{0, 3, 6};
                  break;
            }
      return std::set<int>();
      }

std::set<int> chordTypeToTpcSet(const MidiChord &chord)
      {
      std::set<int> tpcSet;
      for (const MidiNote& note: chord.notes)
            tpcSet.insert(note.pitch % 12);
      return tpcSet;
      }

void recognizeChordNames(const std::multimap<ReducedFraction, MidiChord> &allChords)
      {
      if (allChords.empty())
            return;

      std::set<ChordNameSegment> segments;

      for (auto it = allChords.begin(); it != allChords.end(); ++it) {
            if (it->second.notes.size() >= 3)
                  segments.insert(ChordNameSegment(it, std::next(it)));
            }

      for (auto it = segments.begin(); it != segments.end(); ++it) {
            const auto nextSeg = std::next(it);
            // between segments there are chords that do not belong to any segment,
            //  so put these chords into a new segment
            if (it->end() != nextSeg->start())
                  it = segments.insert(ChordNameSegment(it->end(), nextSeg->start())).first;
            }
      }

*/

} // namespace MidiChordName
} // namespace Ms
