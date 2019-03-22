#ifndef MIQLINEEDIT_H
#define MIQLINEEDIT_H

#include <QLineEdit>
#include <QMouseEvent>

class MiQLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    MiQLineEdit(QWidget * parent = 0);
    MiQLineEdit(const MiQLineEdit & linea);
    MiQLineEdit& operator=(const MiQLineEdit & linea);

protected:
    void mousePressEvent(QMouseEvent *);


};

#endif // MIQLINEEDIT_H
