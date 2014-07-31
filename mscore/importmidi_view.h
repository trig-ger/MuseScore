#ifndef IMPORTMIDI_VIEW_H
#define IMPORTMIDI_VIEW_H

#include <QTableView>


namespace Ms {

class SeparatorDelegate : public QStyledItemDelegate
      {
   public:
      SeparatorDelegate(QObject *parent = 0)
            : QStyledItemDelegate(parent)
            , _frozenRowIndex(-1)
            , _frozenColIndex(-1)
            {}

      void setFrozenRowIndex(int index)
            {
            _frozenRowIndex = index;
            }

      void setFrozenColIndex(int index)
            {
            _frozenColIndex = index;
            }

      void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
            {
            this->QStyledItemDelegate::paint(painter, option, index);

            if (index.row() == _frozenRowIndex) {
                  painter->save();
                  painter->setPen(option.palette.foreground().color());
                  painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
                  painter->restore();
                  }
            if (index.column() == _frozenColIndex) {
                  painter->save();
                  painter->setPen(option.palette.foreground().color());
                  painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
                  painter->restore();
                  }
            }
   private:
      int _frozenRowIndex;
      int _frozenColIndex;
      };

class TracksModel;

class TracksView : public QTableView
      {
      Q_OBJECT

   public:
      TracksView(QWidget *parent);
      ~TracksView();

      void setModel(TracksModel *model);

      void setItemDelegate(SeparatorDelegate *delegate);

      void restoreHHeaderState(const QByteArray &data);
      void restoreVHeaderState(const QByteArray &data);
      void setHHeaderResizeMode(QHeaderView::ResizeMode mode);
      void setVHeaderDefaultSectionSize(int size);
      void resetHHeader();
      void resetVHeader();

   protected:
      void resizeEvent(QResizeEvent *event);
      bool viewportEvent(QEvent *event);
      void wheelEvent(QWheelEvent *event);

   private slots:
      void currentChanged(const QModelIndex &, const QModelIndex &);

      void updateMainViewSectionWidth(int,int,int);
      void updateMainViewSectionHeight(int,int,int);
      void updateFrozenSectionWidth(int,int,int);
      void updateFrozenSectionHeight(int,int,int);
      void onHSectionMove(int,int,int);
      void onVSectionMove(int,int,int);
      void updateFrozenRowCount();
      void updateFrozenColCount();

   private:
      void initHorizontalView();
      void initVerticalView();
      void initCornerView();
      void initMainView();
      void initConnections();
      void setupEditTriggers();
      void updateFrozenTableGeometry();

      void keepVisible(const QModelIndex &previous, const QModelIndex &current);

      int frozenRowHeight();
      int frozenColWidth();

      int frozenVTableWidth();
      int frozenHTableHeight();

      void updateFocus(int currentRow, int currentColumn);

      QTableView *_frozenHTableView;
      QTableView *_frozenVTableView;
      QTableView *_frozenCornerTableView;
      SeparatorDelegate *_delegate;
      };

} // namespace Ms


#endif // IMPORTMIDI_VIEW_H
