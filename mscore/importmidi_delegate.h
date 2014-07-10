#ifndef IMPORTMIDI_DELEGATE_H
#define IMPORTMIDI_DELEGATE_H


namespace Ms {

class SizedListWidget : public QListWidget
      {
      Q_OBJECT

   public:
      SizedListWidget(QWidget *parent = nullptr);

      virtual QSize sizeHint () const;
      };

// class for multiple value representation
// each value is a button that can be checked or unchecked

class MultiValue : public QWidget
      {
      Q_OBJECT

   public:
      MultiValue(const QStringList &values, QWidget *parent = nullptr);

      QStringList data() const;

   signals:
      void okClicked();

   private slots:
      void checkBoxClicked(QListWidgetItem *);
      void itemClicked(QListWidgetItem *);

   private:
      void setAllCheckBox();
      void updateStates();
      void connectCheckBoxes();
      void disconnectCheckBoxes();

      SizedListWidget *_listWidget;
      std::vector<Qt::CheckState> _states;
      };


class OperationsDelegate : public QStyledItemDelegate
      {
      Q_OBJECT

   public:
      explicit OperationsDelegate(QWidget *appWindow, bool rightArrowAlign);

      void paint(QPainter *painter,
                 const QStyleOptionViewItem &option,
                 const QModelIndex &index) const;

      QWidget* createEditor(QWidget *parent,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const;

      void setEditorData(QWidget *editor, const QModelIndex &index) const;
      void setModelData(QWidget *editor, QAbstractItemModel *model,
                        const QModelIndex &index) const;

   private slots:
      void commitAndCloseEditor();

   private:
      void drawArrow(QPainter *painter,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) const;

      QWidget *appWindow;
      const bool rightArrowAlign;
      };

} // namespace Ms


#endif // IMPORTMIDI_DELEGATE_H
