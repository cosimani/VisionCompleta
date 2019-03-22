#include "miqlineedit.h"
#include <QFileDialog>

MiQLineEdit::MiQLineEdit(QWidget *parent) : QLineEdit(parent)
{

}

MiQLineEdit::MiQLineEdit(const MiQLineEdit &linea) : QLineEdit()  {
    this->setText(linea.text());
}

MiQLineEdit &MiQLineEdit::operator=(const MiQLineEdit &linea)  {
    this->setText(linea.text());
    return *this;
}

void MiQLineEdit::mousePressEvent(QMouseEvent *)
{
    this->setText(QFileDialog::getOpenFileName(this, "Elija el recurso", "../RA-UBP", "Multimedia (*.png *.mp4 *.jpg)"));
}


