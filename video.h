#ifndef VIDEO_H
#define VIDEO_H

#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QMediaPlayer>
#include <QGLWidget>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

class Grabber : public QAbstractVideoSurface  {
    Q_OBJECT

public:
    GLuint textureId;

    Grabber( QObject *parent = 0 ) : QAbstractVideoSurface( parent ), textureId( 0 )  {
        glGenTextures( 1, &textureId );
    }

    QList< QVideoFrame::PixelFormat > supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle ) const
    {
        if( handleType == QAbstractVideoBuffer::NoHandle )  {
             return QList< QVideoFrame::PixelFormat >() << QVideoFrame::Format_RGB32
                                                        << QVideoFrame::Format_ARGB32
                                                        << QVideoFrame::Format_ARGB32_Premultiplied
                                                        << QVideoFrame::Format_RGB32
                                                        << QVideoFrame::Format_RGB24
                                                        << QVideoFrame::Format_BGRA32
                                                        << QVideoFrame::Format_BGRA32_Premultiplied
                                                        << QVideoFrame::Format_BGR32
                                                        << QVideoFrame::Format_Jpeg
                                                        << QVideoFrame::Format_CameraRaw
                                                        << QVideoFrame::Format_AdobeDng;

        }
        else  {
            return QList< QVideoFrame::PixelFormat >();
        }
    }

    bool start( const QVideoSurfaceFormat &format )  {
        const QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat( format.pixelFormat() );
        const QSize size = format.frameSize();

        if ( imageFormat != QImage::Format_Invalid && !size.isEmpty() )  {
            QAbstractVideoSurface::start( format );
            return true;
        }
        else  {
            return false;
        }
    }

    bool present( const QVideoFrame &frame )  {
        QVideoFrame cloneFrame( frame );

        if ( ! cloneFrame.map( QAbstractVideoBuffer::ReadOnly ))  {
            return false;
        }

        glBindTexture( GL_TEXTURE_2D, this->textureId );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D( GL_TEXTURE_2D, 0, 3, cloneFrame.width(), cloneFrame.height(),
                      0, GL_BGRA, GL_UNSIGNED_BYTE, cloneFrame.bits() );

        return true;
    }
};

class Video : public QObject  {
    Q_OBJECT

public:
    QMediaPlayer *player;
    Grabber *grabber;
    QString name;
    int volume;

    void setPosition( int position )  {
        player->setPosition( position );
        qDebug() << "video rebobinado";
    }

    bool isReproduciendo()  {
        if ( player->position() > 0 )
            return true;
        else
            return false;
    }

    Video( QString name, QObject *parent = 0 ) : QObject( parent ),
                                                 player( new QMediaPlayer( this ) ),
                                                 grabber( new Grabber( this ) ),
                                                 name( QFileInfo(name).fileName() ),
                                                 volume( 100 )
    {
//        QString videoUri = QDir::currentPath() + "/../Videos/" + name;
        QString videoUri = name;

        if( QFile::exists( videoUri ) )  {
            player->setVideoOutput( grabber );
            player->setMedia( QUrl::fromLocalFile( videoUri ) );
        }
    }
};

#endif // VIDEO_H
