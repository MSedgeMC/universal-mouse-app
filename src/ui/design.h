#pragma once
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
namespace mouse {
QIcon designIcon(const QString &name, QColor color = QColor("#b7b9be"),
                 int size = 22);
QLabel *designLabel(const QString &text, const QString &role = {});
QFrame *designCard(QVBoxLayout **layout, const QString &title = {},
                   const QString &subtitle = {});
void applyDesign(QWidget *);
class LevelSpin : public QSpinBox {
public:
  explicit LevelSpin(bool brightness);

protected:
  QString textFromValue(int value) const override;

private:
  bool brightness;
};
class MacroEventDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *, const QStyleOptionViewItem &,
             const QModelIndex &) const override;
  QSize sizeHint(const QStyleOptionViewItem &,
                 const QModelIndex &) const override;
};
} // namespace mouse
