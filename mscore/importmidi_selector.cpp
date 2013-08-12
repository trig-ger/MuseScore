#include "importmidi_selector.h"
#include "ui_importmidi_selector.h"


namespace Ms {

TupletSelector::TupletSelector(QWidget *parent)
      : QWidget(parent)
      , ui(new Ui::TupletSelector)
      {
      ui->setupUi(this);

      ui->pushButtonDuplets->setChecked(tuplets.duplets);
      ui->pushButtonTriplets->setChecked(tuplets.triplets);
      ui->pushButtonQuadruplets->setChecked(tuplets.quadruplets);
      ui->pushButtonQuintuplets->setChecked(tuplets.quintuplets);
      ui->pushButtonSeptuplets->setChecked(tuplets.septuplets);
      ui->pushButtonNonuplets->setChecked(tuplets.nonuplets);

      connect(ui->pushButtonDuplets, SIGNAL(toggled(bool)), SLOT(setDuplets(bool)));
      connect(ui->pushButtonTriplets, SIGNAL(toggled(bool)), SLOT(setTriplets(bool)));
      connect(ui->pushButtonQuadruplets, SIGNAL(toggled(bool)), SLOT(setQuadruplets(bool)));
      connect(ui->pushButtonQuintuplets, SIGNAL(toggled(bool)), SLOT(setQuintuplets(bool)));
      connect(ui->pushButtonSeptuplets, SIGNAL(toggled(bool)), SLOT(setSeptuplets(bool)));
      connect(ui->pushButtonNonuplets, SIGNAL(toggled(bool)), SLOT(setNonuplets(bool)));
      }

TupletSelector::~TupletSelector()
      {
      delete ui;
      }

void TupletSelector::setDuplets(bool value)
      {
      tuplets.duplets = value;
      }

void TupletSelector::setTriplets(bool value)
      {
      tuplets.triplets = value;
      }

void TupletSelector::setQuadruplets(bool value)
      {
      tuplets.quadruplets = value;
      }

void TupletSelector::setQuintuplets(bool value)
      {
      tuplets.quintuplets = value;
      }

void TupletSelector::setSeptuplets(bool value)
      {
      tuplets.septuplets = value;
      }

void TupletSelector::setNonuplets(bool value)
      {
      tuplets.nonuplets = value;
      }

} // namespace Ms
