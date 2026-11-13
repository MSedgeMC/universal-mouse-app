#pragma once
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>
namespace mouse {
QIcon designIcon(const QString &name, QColor color = QColor("#9eabbc"),
                 int size = 22);
QLabel *designLabel(const QString &text, const QString &role = {});
QFrame *designCard(QVBoxLayout **layout, const QString &title = {},
                   const QString &subtitle = {});
void applyDesign(QWidget *);
} // namespace mouse
