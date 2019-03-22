#ifndef PRINCIPAL_H
#define PRINCIPAL_H

#include <QWidget>

namespace Ui  {
    class Principal;
}

class Principal : public QWidget  {
    Q_OBJECT

public:
    explicit Principal(QWidget *parent = 0);
    ~Principal();
    void setVisibleSliders(bool visible);

    Ui::Principal *ui;  // Lo hice publico para poder acceder al ui desde Scene

private:
    void cargarCamaras();

private slots:
    void slot_cbCamarasChanged(int nuevoIndex);
    void slot_vincular();
    void slot_desvincularTodo();

signals:
    void signal_vincular( int, QString, QString );
};

#endif // PRINCIPAL_H
