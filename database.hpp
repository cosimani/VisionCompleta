#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <QObject>
#include <QDebug>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QVector>

class Database: public QObject
{
    Q_OBJECT

private:
    static Database *instance;
    explicit Database( QObject *parent = NULL );

    QSqlDatabase database;

    bool connectDatabase();
    void disconnectDatabase();

public:
    static Database *getInstance();
    ~Database();

    int checkBase();

    bool saveVinculo(int marker_id, QString recurso , QString formatoCaja = "n");
    QVector<QStringList> readVinculos();

    bool desvincularTodo();
};

#endif // DATABASE_HPP
