#include "database.hpp"
#include <QDebug>
#include <QSqlError>
#include "common.h"

Database *Database::instance = NULL;

Database::Database( QObject *parent ) :
    QObject( parent )
{
    database = QSqlDatabase::addDatabase( "QSQLITE" );
}

Database *Database::getInstance()
{
    if( !instance )
    {
        instance = new Database();
    }
    return instance;
}

Database::~Database()
{
    delete instance;
}

bool Database::connectDatabase()
{
#ifdef PORTABLE
    database.setDatabaseName( "./Files/db.sqlite" );
#else
    database.setDatabaseName( APPLICATION_PATH "Files/db.sqlite" );
#endif
    return database.open();
}

void Database::disconnectDatabase()
{
    database.close();
}

int Database::checkBase()
{
    if( this->connectDatabase() )
    {
        if( database.tables().contains( "vinculos" ) )
        {
            this->disconnectDatabase();
            return 1;
        }
        else
        {
            QSqlQuery query( database );

            QString queryString( "create table vinculos                            "
                                 "(                                                "
                                 "    marker_id      integer         primary key,  "
                                 "    recurso        varchar(100)    not null,     "
                                 "    formato_caja   varchar(5)      null          "
                                 ")" );

            bool ok = query.exec( queryString );

            qDebug() << "metodo checkBase()" << query.lastError() << query.lastQuery();

            this->disconnectDatabase();

            return ok ? 0 : -1;
        }
    }
    else
    {
        return -1;  // No se logro conectar a la base
    }
}

bool Database::saveVinculo( int marker_id, QString recurso, QString formatoCaja )
{
    if( this->connectDatabase() )
    {
        QSqlQuery query( database );

        QString queryString( "INSERT OR REPLACE INTO vinculos (marker_id, recurso, formato_caja) "
                             "VALUES(" + QString::number( marker_id ) + ", '" + recurso + "', '" + formatoCaja + "');" );

        bool ok = query.exec( queryString );

        qDebug() << "metodo saveVinculo()" << query.lastError() << query.lastQuery();

        this->disconnectDatabase();
        return ok;
    }
    else
    {
        return false;
    }
}

QVector<QStringList> Database::readVinculos()
{
    QVector<QStringList> results;

    if( this->connectDatabase() )
    {
        QSqlQuery query = database.exec( "select marker_id, recurso, formato_caja from vinculos" );

        while ( query.next() )
        {
            QStringList registro;
            registro.append( query.value( 0 ).toString() );  // marker_id
            registro.append( query.value( 1 ).toString() );  // recurso
            registro.append( query.value( 2 ).toString() );  // formato_caja
            results.append(registro);
        }

        this->disconnectDatabase();
    }

    return results;
}

bool Database::desvincularTodo()
{
    if( this->connectDatabase() )
    {
        QSqlQuery query( database );

        QString queryString( "DELETE FROM vinculos" );

        bool ok = query.exec( queryString );

        qDebug() << "metodo saveVinculo()" << query.lastError() << query.lastQuery();

        this->disconnectDatabase();
        return ok;
    }
    else
    {
        return false;
    }
}
