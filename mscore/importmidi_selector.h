#ifndef IMPORTMIDI_SELECTOR_H
#define IMPORTMIDI_SELECTOR_H

#include "importmidi_operations.h"


namespace Ui {
class TupletSelector;
}

namespace Ms {

class TupletSelector : public QWidget
      {
      Q_OBJECT

   public:
      explicit TupletSelector(QWidget *parent = 0);
      ~TupletSelector();

      SearchTuplets tupletSelection() const { return tuplets; }

   private slots:
      void setDuplets(bool value);
      void setTriplets(bool value);
      void setQuadruplets(bool value);
      void setQuintuplets(bool value);
      void setSeptuplets(bool value);
      void setNonuplets(bool value);

   private:
      Ui::TupletSelector *ui;
      SearchTuplets tuplets;
      };

} // namespace Ms


#endif // IMPORTMIDI_SELECTOR_H
