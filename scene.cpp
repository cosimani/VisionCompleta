#include "scene.h"
#include <QApplication>
#include "ui_principal.h"  // Necesario para acceder al ui de Principal

#include "database.hpp"
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include "common.h"

Scene::Scene( QWidget *parent ) : QGLWidget( parent ),
                                  nCamera( 0 ),

//                                  videoCapture ( new cv::VideoCapture( nCamera ) ),

                                  sceneTimer ( new QTimer ),
                                  milisecondsTimer( 10 ),

                                  textures( new QVector< Texture * > ),
                                  texturesVinculadas( new QVector< Texture * > ),
                                  models( new QVector< Model * > ),
                                  videos( new QVector< Video * > ),
                                  videosVinculados( new QVector< Video * > ),

                                  markerDetector( new MarkerDetector ),
                                  cameraParameters( new CameraParameters ),

                                  zRotationVelocity( 0 ),

                                  principal ( (Principal*) parent ),
                                  rebobinarTodosLosVideos( false ),
                                  tfNetwork( new cv::dnn::Net ),
                                  inWidth( 300 ),
                                  inHeight( 300 ),
                                  meanVal( 127.5 ),  // 255 divided by 2
                                  inScaleFactor( 1.0f / meanVal ),
                                  confidenceThreshold( 0.69f )
{
//    this->setFixedSize( videoCapture->get( CV_CAP_PROP_FRAME_WIDTH ), videoCapture->get( CV_CAP_PROP_FRAME_HEIGHT ) );

    videoCapture = new cv::VideoCapture();

    *tfNetwork = cv::dnn::readNetFromTensorflow( APPLICATION_PATH "clasificadores/frozen_inference_graph.pb",
                                                 APPLICATION_PATH "clasificadores/frozen_inference_graph.pbtxt" );

    QFile labelsFile( APPLICATION_PATH "clasificadores/labels.txt" );
    if( labelsFile.open( QFile::ReadOnly | QFile::Text) )
    {
        while( ! labelsFile.atEnd() )
        {
            QString line = labelsFile.readLine();
            classNames[ line.split( ',' )[ 0 ].trimmed().toInt() ] = line.split( ',' )[ 1 ].trimmed();
        }
        labelsFile.close();
    }

//    QMessageBox::information(this, "es", "dasd"+QDir::currentPath());

#ifdef PORTABLE
    QFile file( "Files/CameraParameters.yml" );
    if ( !file.exists() )  {
        QMessageBox::critical(this, "No se puede configurar la camara", "Falta el archivo CameraParameters.yml.");
        std::exit(0);
    }
    cameraParameters->readFromXMLFile( "Files/CameraParameters.yml" );
#else
    QFile file( APPLICATION_PATH "Files/CameraParameters.yml" );
    if ( !file.exists() )  {
        QMessageBox::critical(this, "No se puede configurar la camara", "Falta el archivo CameraParameters.yml.");
        std::exit(0);
    }
    cameraParameters->readFromXMLFile( APPLICATION_PATH "Files/CameraParameters.yml" );
#endif

    if ( ! cameraParameters->isValid() )  {

    }

    sceneTimer->start( milisecondsTimer );
    connect( sceneTimer, SIGNAL( timeout() ), SLOT( slot_updateScene() ) );

    Database::getInstance()->checkBase();

//    principal->setVisibleSliders(false);

}

Scene::~Scene()
{
    videoCapture->release();
}

void Scene::actualizarTexturas()
{
    texturesVinculadas->clear();
    videosVinculados->clear();

    imageFiles = Database::getInstance()->readVinculos();

    for ( int i = 0; i < imageFiles.size(); i++ )
    {
        QFileInfo fileInfo = imageFiles.at( i ).at(1);
        QString fileName = fileInfo.fileName();

        if ( fileInfo.suffix() != "mp4" )  {  // Entra si es png

            texturesVinculadas->append( new Texture( fileName ) );
            QString textureUri = imageFiles.at( i ).at(1);

            qDebug() << "texture Uri" << textureUri;

            Mat textureMat = imread( textureUri.toStdString(), CV_LOAD_IMAGE_COLOR );

//            cv::cvtColor(textureMat, textureMat, )
    //        flip( textureMat, textureMat, 0 );
            texturesVinculadas->last()->mat = textureMat;
            texturesVinculadas->last()->generateFromMat();
        }
        else  {  // Entra si es mp4

//            videosVinculados->append( new Video( fileName ) );
            videosVinculados->append( new Video( fileInfo.absoluteFilePath() ) );
        }
    }


}


void Scene::loadTextures()
{
#ifdef PORTABLE
    QDir directory( "./Textures" );
#else
    QDir directory( APPLICATION_PATH "Textures" );
#endif

    QStringList fileFilter;
    fileFilter << "*.jpg" << "*.png" << "*.bmp" << "*.gif";
    QStringList imageFiles = directory.entryList( fileFilter );

    for ( int i = 0; i < imageFiles.size(); i++ )
    {
        textures->append( new Texture( imageFiles.at( i ) ) );
        QString textureUri = APPLICATION_PATH "Textures/" + imageFiles.at( i );

        Mat textureMat = imread( textureUri.toStdString() );
        flip( textureMat, textureMat, 0 );
        textures->last()->mat = textureMat;
        textures->last()->generateFromMat();
    }
}

void Scene::loadModels()
{
#ifdef PORTABLE
    QDir directory( "./Models" );
#else
    QDir directory( APPLICATION_PATH "Models" );
#endif

    QStringList fileFilter;
    fileFilter << "*.3ds";
    QStringList modelFiles = directory.entryList( fileFilter );

    for ( int i = 0 ; i < modelFiles.size() ; i++ )
        models->append( new Model( modelFiles.at( i ) ) );

    prepareModels();

}

void Scene::prepareModels()
{
    loadTexturesForModels();

    for ( int i = 0 ; i < models->size() ; i++)
    {
        if( !models->at( i ) ) return;

        models->at( i )->getFaces();
        Lib3dsVector *vertices = new Lib3dsVector[ models->at( i )->totalFaces * 3 ];
        Lib3dsVector *normals = new Lib3dsVector[ models->at( i )->totalFaces * 3 ];
        Lib3dsTexel *texCoords = new Lib3dsTexel[ models->at( i )->totalFaces * 3 ];
        Lib3dsMesh *mesh;

        unsigned int finishedFaces = 0;

        for( mesh = models->at(i)->model->meshes; mesh != NULL ; mesh = mesh->next )
        {
            lib3ds_mesh_calculate_normals( mesh, &normals[ finishedFaces * 3 ] );
            for( unsigned int currentFace = 0; currentFace < mesh->faces ; currentFace++ )
            {
                Lib3dsFace * face = &mesh->faceL[ currentFace ];
                for( unsigned int i = 0; i < 3; i++ )
                {
                    if( &mesh->texelL )
                        memcpy( &texCoords[ finishedFaces*3 + i ],
                                mesh->texelL[ face->points[ i ] ],
                                sizeof( Lib3dsTexel ) );

                    memcpy( &vertices[ finishedFaces * 3 + i ],
                            mesh->pointL[ face->points[ i ] ].pos,
                            sizeof( Lib3dsVector ) );
                }
                finishedFaces++;
            }
        }

        glGenBuffers( 1, &models->at(i)->vertexVBO );
        glBindBuffer( GL_ARRAY_BUFFER, models->at(i)->vertexVBO );
        glBufferData( GL_ARRAY_BUFFER, sizeof( Lib3dsVector ) * 3 * models->at(i)->totalFaces, vertices, GL_STATIC_DRAW );

        glGenBuffers( 1, &models->at(i)->normalVBO );
        glBindBuffer( GL_ARRAY_BUFFER, models->at(i)->normalVBO);
        glBufferData( GL_ARRAY_BUFFER, sizeof( Lib3dsVector ) * 3 * models->at(i)->totalFaces, normals, GL_STATIC_DRAW );

        glGenBuffers( 1, &models->at(i)->texCoordVBO );
        glBindBuffer( GL_ARRAY_BUFFER, models->at(i)->texCoordVBO);
        glBufferData( GL_ARRAY_BUFFER, sizeof( Lib3dsTexel ) * 3 * models->at(i)->totalFaces, texCoords, GL_STATIC_DRAW );

        delete vertices;
        delete normals;
        delete texCoords;

        lib3ds_file_free( models->at(i)->model );
        models->at(i)->model = NULL;
    }
}

void Scene::loadTexturesForModels()
{
    for ( int i = 0 ; i < models->size(); i++ )
    {
        QString modelTextureName = models->at( i )->name;
        modelTextureName.remove( ".3ds" );
        modelTextureName += ".jpg";

        for( int j = 0; j < textures->size(); j++ )
            if( textures->at( j )->name == modelTextureName )
                models->operator []( i )->textureId = textures->at( j )->id;
    }
}

void Scene::loadVideos()
{
#ifdef PORTABLE
    QDir directory( "./Videos" );
#else
    QDir directory( APPLICATION_PATH "Videos" );
#endif

    QStringList fileFilter;
    fileFilter << "*.avi" << "*.wmv" << "*.mpg" << "*.mpeg" << "*.mpeg1" << "*.mpeg2" << "*.mpeg4" << "*.mp4";
    QStringList videoFiles = directory.entryList( fileFilter );

    for ( int i = 0 ; i < videoFiles.size() ; i++ )
        videos->append( new Video( videoFiles.at( i ) ) );
}

void Scene::initializeGL()
{
    initializeGLFunctions();

    glClearColor( 0, 0, 0, 0 );
    glShadeModel( GL_SMOOTH );
    glEnable( GL_DEPTH_TEST );

    GLfloat lightAmbient[4]; lightAmbient[0] = 0.5f;  lightAmbient[1] = 0.5f;
            lightAmbient[2] = 0.5f;  lightAmbient[3] = 1.0f;

    GLfloat lightDiffuse[4]; lightDiffuse[0] = 1.0f;  lightDiffuse[1] = 1.0f;
            lightDiffuse[2] = 1.0f;  lightDiffuse[3] = 1.0f;

    GLfloat lightPosition[4];lightPosition[0]= 0.0f;  lightPosition[1]= 0.0f;
            lightPosition[2]= 2.0f;  lightPosition[3]= 1.0f;

    glLightfv( GL_LIGHT1, GL_AMBIENT, lightAmbient );  glLightfv( GL_LIGHT1, GL_DIFFUSE, lightDiffuse );
    glLightfv( GL_LIGHT1, GL_POSITION,lightPosition ); glEnable( GL_LIGHT1 );

    textures->append( new Texture( "CameraTexture" ) );

    loadTextures();
    emit message( "Texturas cargadas" );

    actualizarTexturas();
    emit message( "Texturas vinculadas cargadas" );

    loadModels();
    emit message( "Modelos cargados" );

    loadVideos();
    emit message( "Videos cargados" );
}

void Scene::resizeGL( int width, int height )
{
    glViewport( 0, 0, width, height );
}

void Scene::paintGL()
{
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    glOrtho( 0, RESOLUTION_WIDTH, 0, RESOLUTION_HEIGHT, 1, 1000 );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    // Inicio: Gráfico de cámara

    glEnable( GL_TEXTURE_2D );
    glColor3f( 1, 1, 1 );
    glBindTexture( GL_TEXTURE_2D, textures->at( 0 )->id );
    glBegin( GL_QUADS );

        glTexCoord2f( 0, 0 ); glVertex3f( 0, RESOLUTION_HEIGHT, -999 );
        glTexCoord2f( 1, 0 ); glVertex3f( RESOLUTION_WIDTH, RESOLUTION_HEIGHT, -999 );
        glTexCoord2f( 1, 1 ); glVertex3f( RESOLUTION_WIDTH, 0, -999 );
        glTexCoord2f( 0, 1 ); glVertex3f( 0, 0, -999 );

    glEnd();
    glDisable( GL_TEXTURE_2D );

    // Fin: Gráfico de cámara

    glMatrixMode( GL_PROJECTION );
    double projectionMatrix[16];

    cv::Size2i sceneSize( RESOLUTION_WIDTH, RESOLUTION_HEIGHT );
    cv::Size2i openGlSize( RESOLUTION_WIDTH, RESOLUTION_HEIGHT );
    cameraParameters->glGetProjectionMatrix( sceneSize, openGlSize, projectionMatrix, 0.05, 10 );

    glLoadMatrixd( projectionMatrix );
    glMatrixMode( GL_MODELVIEW );
    double modelview_matrix[16];

    // Si este check box esta seteado, entocnes no mostrar el contenido virtual
    if ( principal->ui->cbSinContenido->isChecked() )
        return;

    // Inicio: Gráfico en marcadores

    for( int i = 0 ; i < detectedMarkers.size() ; i++ )
    {
        detectedMarkers.operator []( i ).glGetModelViewMatrix( modelview_matrix );
        glLoadMatrixd( modelview_matrix );

        // Rotacion activada a traves del checkbox
        if( principal->ui->rotateCheckBox->isChecked() )  {
            zRotationVelocity += principal->ui->rotationVelocitySlider->value();
            glRotatef( zRotationVelocity, 0, 0, 1 );
        }

//        emit message( QString::number(detectedMarkers.at( i ).id) );

        float coefTamano = (float)principal->ui->sbTamano->value() / (float)100;

//        QVector<QStringList> vTexVinculadas = Database::getInstance()->readVinculos();
        QVector<QStringList> vTexVinculadas = imageFiles;

        for ( int j=0 ; j<vTexVinculadas.size() ; j++ )  {

            if ( detectedMarkers.at( i ).id == vTexVinculadas.at( j ).at(0).toInt() )  {

                if ( vTexVinculadas.at( j ).at(1) == "Casa" )  {
                    glRotatef(90, 1,0,0);
                    drawModel( "House.3ds", 25 * coefTamano);
                }
                else if ( vTexVinculadas.at( j ).at(1) == "Hombre" )  {
                    glRotatef(180, 1,0,0);
                    drawModel( "Man.3ds", 4 * coefTamano );
                }
                else if ( vTexVinculadas.at( j ).at(1) == "Oil" )  {
                    glRotatef(180, 1,0,0);
                    drawModel( "Oil.3ds", 80 * coefTamano );
                }
                else if ( vTexVinculadas.at( j ).at(1) == "Manzana" )  {
                    glRotatef(180, 1,0,0);
                    drawModel( "appleD.3ds", 4 * coefTamano );
                }
                else if ( vTexVinculadas.at( j ).at(1) == "Profesora" )  {
                    glRotatef(180, 1,0,0);
                    drawModel( "profesora.3ds", 200 * coefTamano );
                }
                else if ( vTexVinculadas.at( j ).at(1) == "Iphone" )  {
                    glRotatef(180, 1,0,0);
                    drawModel( "IPhone.3ds", 80 * coefTamano );
                }
                else  {
                    QFileInfo fileInfo = vTexVinculadas.at( j ).at(1);
                    QString fileName = fileInfo.fileName();

                    if ( fileInfo.suffix() == "mp4" )  {
                        // Es un aproximado para el volumen segun el area del marcador
                        int volumen = ((int)detectedMarkers.at( i ).getArea()) / 300;
                        drawVideoVinculado( fileName, detectedMarkers.at( i ).ssize, 160 * coefTamano, volumen );
                    }
                    else  {
                        if ( vTexVinculadas.at( j ).at(2) == "n" )  {
                            drawSheetVinculadas( fileName, detectedMarkers.at( i ).ssize, 100 * coefTamano );
                        }
                        else
                            drawBoxVinculado( fileName, detectedMarkers.at( i ).ssize, 100 * coefTamano );
                    }

                }


            }

        }



//        switch( detectedMarkers.at( i ).id )  {

//        case 459: {
//            drawBox( "Danger.jpg", detectedMarkers.at( i ).ssize, 100 );
//            break; }

//        case 56: {
//            drawSheet( "messi.png", detectedMarkers.at( i ).ssize, 100 );
//            break; }

//        case 58: {
//            // Es un aproximado para el volumen segun el area del marcador
//            int volumen = ((int)detectedMarkers.at( i ).getArea()) / 300;
//            drawVideo( "trailer-relato.mp4", detectedMarkers.at( i ).ssize, 160, volumen );
//            break; }

//        case 55: {
//            // Es un aproximado para el volumen segun el area del marcador
//            int volumen = ((int)detectedMarkers.at( i ).getArea()) / 300;
//            drawVideo( "trailer-RF7.mp4", detectedMarkers.at( i ).ssize, 160, volumen );
//            break; }

//        case 57: {
//            glRotatef(90, 1,0,0);
//            drawModel( "House.3ds", 25 );
//            break; }

//        case 59: {
//            glRotatef(180, 1,0,0);
//            drawModel( "Man.3ds", 4 );
//            break; }

//        default: { break; }
//        }


    }

    // La siguiente linea se ejecuta siempre. Habria que ingeniarsela de otra forma para bajar el volumen
    decreaseVideosVolume();
    decreaseVideosVolumeVinculados();

    // Fin: Gráfico en marcadores

    glFlush();

}

void Scene::keyPressEvent( QKeyEvent *event )
{
    switch( event->key() )  {
    case Qt::Key_M:
        if ( principal->isFullScreen() )  {
            principal->showMaximized();
            principal->setVisibleSliders(true);
        }
        else  {
            principal->showFullScreen();
            principal->setVisibleSliders(false);
        }
        break;

    case Qt::Key_R:
        for ( int i=0 ; i < videosVinculados->size() ; i++ )  {
            if ( videosVinculados->at( i )->isReproduciendo() )
                videosVinculados->at( i )->setPosition( 0 );
        }

        break;

    case Qt::Key_P:
        for ( int i=0 ; i < videosVinculados->size() ; i++ )  {
            if ( videosVinculados->at( i )->isReproduciendo() )
                videosVinculados->at( i )->player->pause();
        }

        break;


    case Qt::Key_Escape:
        qApp->quit();
        break;

    default:;
    }
}



void Scene::process( Mat &frame )
{
    Mat detections;

    // Entra a este if cuando esta chequeado el checkbox, para reconocer cosas con tensorflow
    if ( principal->ui->cbReconocerCosas->isChecked() )  {
        Mat frameTensorFlow;
        cvtColor( frame, frameTensorFlow, CV_BGR2RGB );

        Mat inputBlob = cv::dnn::blobFromImage( frameTensorFlow,
                                                inScaleFactor,
                                                Size( inWidth, inHeight ),
                                                Scalar( meanVal, meanVal, meanVal ),
                                                true,
                                                false );
        tfNetwork->setInput( inputBlob );
        Mat result = tfNetwork->forward();
        detections = Mat( result.size[ 2 ], result.size[ 3 ], CV_32F, result.ptr< float >() );
    }


    Mat grayscaleMat; cvtColor( frame, grayscaleMat, CV_BGR2GRAY );
    Mat binaryMat; threshold( grayscaleMat, binaryMat, 128, 255, cv::THRESH_BINARY );

    std::vector< Marker > detectedMarkersVector;
    cameraParameters->resize( binaryMat.size() );
    markerDetector->detect( binaryMat, detectedMarkersVector, *cameraParameters, 0.08f );
    detectedMarkers = QVector< Marker >::fromStdVector( detectedMarkersVector );

    // descripcion del marker
    if ( principal->ui->cbMostrarId->isChecked() )
        for( int i = 0; i < detectedMarkers.size(); i++ )
            detectedMarkers.at( i ).draw( frame, Scalar( 255, 0, 255 ), 1 );


    // Entra a este for cuando se detectaron objetos con tensorflow
    for( int i = 0; i < detections.rows; i++ )
    {
        float confidence = detections.at< float >( i, 2 );  // En la columna 2 esta la confianza

        // De todos los objetos que haya en labels.txt, va a entrar a este if solos aquellos
        // que superen el umbral de confianza o de probabilidad de que sea el objeto.
        if( confidence > confidenceThreshold )
        {
            int objectClass = ( int )( detections.at< float >( i, 1 ) );  // En la columna 1 esta el id del objeto

            int left = static_cast<int>(detections.at<float>(i, 3) * frame.cols);
            int top = static_cast<int>(detections.at<float>(i, 4) * frame.rows);
            int right = static_cast<int>(detections.at<float>(i, 5) * frame.cols);
            int bottom = static_cast<int>(detections.at<float>(i, 6) * frame.rows);

            rectangle( frame, Point( left, top ), Point( right, bottom ), Scalar( 0, 255, 0 ) );
            String label = classNames[ objectClass ].toStdString();
            int baseLine = 0;
            Size labelSize = getTextSize( label, FONT_HERSHEY_SIMPLEX, 0.5, 2, &baseLine );
            top = max( top, labelSize.height );
            rectangle( frame, Point( left, top - labelSize.height ),
                       Point( left + labelSize.width, top + baseLine ),
                       Scalar( 255, 255, 255 ), FILLED );
            putText( frame, label, Point( left, top ),
                     FONT_HERSHEY_SIMPLEX, 0.5, Scalar( 0, 0, 0 ) );

        }
    }



}

void Scene::drawCamera( unsigned int percentage )
{
    drawSheet( "CameraTexture", percentage );
}

void Scene::drawCameraBox(unsigned int percentage )
{
    drawBox( "CameraTexture", percentage );
}

/**
 * @brief Scene::drawSheet Dibuja una imagen. (Ver descripcion de Scene::drawBox para mas informacion)
 */
void Scene::drawSheetVinculadas( QString textureName, float sizeMarker, unsigned int percentage )
{
    sizeMarker = sizeMarker * (float)percentage / (float)100;

    for( int i = 0; i < texturesVinculadas->size(); i++ )
    {
        if( texturesVinculadas->at( i )->name == textureName )
        {
            int ancho_textura = texturesVinculadas->at( i )->mat.cols;
//            ancho_textura = 960;
            int alto_textura = texturesVinculadas->at( i )->mat.rows;
//            alto_textura = 540;
            float porcentaje_aspecto = 100 * ( float )ancho_textura / ( float )alto_textura;

            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, texturesVinculadas->at( i )->id );
            glColor3f( 1, 1, 1 );
            glBegin( GL_QUADS );

//            glNormal3f( 0.0f, 0.0f,-1.0f);
//            glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
//            glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );
//            glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );
//            glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );

//                glNormal3f( 0.0f, 0.0f,-1.0f);
//                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker * porcentaje_aspecto / 200, 0 );
//                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2, sizeMarker * porcentaje_aspecto / 200, 0 );
//                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2, sizeMarker * porcentaje_aspecto / 200, 0 );
//                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker * porcentaje_aspecto / 200, 0 );

                glNormal3f( 0.0f, 0.0f,-1.0f);
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker * porcentaje_aspecto / 200, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker * porcentaje_aspecto / 200,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker * porcentaje_aspecto / 200,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker * porcentaje_aspecto / 200, -sizeMarker/2, 0 );

            glEnd();
            glDisable( GL_TEXTURE_2D);
        }
    }
}

/**
 * @brief Scene::drawSheet Dibuja una imagen. (Ver descripcion de Scene::drawBox para mas informacion)
 */
void Scene::drawSheet(QString textureName, float sizeMarker, unsigned int percentage )
{
    sizeMarker = sizeMarker * (float)percentage / (float)100;

    for( int i = 0; i < textures->size(); i++ )
    {
        if( textures->at( i )->name == textureName )
        {
            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, textures->at( i )->id );
            glColor3f( 1, 1, 1 );
            glBegin( GL_QUADS );

                glNormal3f( 0.0f, 0.0f,-1.0f);
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );

            glEnd();
            glDisable( GL_TEXTURE_2D);
        }
    }
}

/**
 * @brief Scene::drawBox Dibuja una caja. Aruco entrega el punto (0,0,0) justo en el centro del marcador.
 * @param textureName Nombre del archivo de imagen en la carpeta Textures. Sera la textura de la caja.
 * @param sizeMarker Tamano del marcador que lo entrega Aruco en el atributo ssize de la clase Marker.
 * @param percentage Para redimensionar la caja. En escala 1:1 la caja tiene el tamano del marcador.
 */
void Scene::drawBox( QString textureName, float sizeMarker, unsigned int percentage )
{
    sizeMarker = sizeMarker * (float)percentage / (float)100;

    for( int i = 0; i < textures->size(); i++ )
    {
        if( textures->at( i )->name == textureName )
        {
            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, textures->at( i )->id );
            glColor3f( 1, 1, 1 );
            glEnable( GL_LIGHTING );
            glBegin( GL_QUADS );

                glNormal3f( 0.0f, 0.0f, 1.0f ); // Techo
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, sizeMarker );

                glNormal3f( 0.0f, 0.0f,-1.0f ); // Piso
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );

                glNormal3f( 0.0f, 1.0f, 0.0f ); // Trasera
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );

                glNormal3f( 0.0f,-1.0f, 0.0f ); // Frontal
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, sizeMarker );

                glNormal3f( 1.0f, 0.0f, 0.0f ); // Derecha
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, sizeMarker );

                glNormal3f( -1.0f, 0.0f, 0.0f ); // Izquierda
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );

            glEnd();
            glDisable( GL_LIGHTING );
            glDisable( GL_TEXTURE_2D );
        }
    }
}

/**
 * @brief Scene::drawBox Dibuja una caja. Aruco entrega el punto (0,0,0) justo en el centro del marcador.
 * @param textureName Nombre del archivo de imagen en la carpeta Textures. Sera la textura de la caja.
 * @param sizeMarker Tamano del marcador que lo entrega Aruco en el atributo ssize de la clase Marker.
 * @param percentage Para redimensionar la caja. En escala 1:1 la caja tiene el tamano del marcador.
 */
void Scene::drawBoxVinculado( QString textureName, float sizeMarker, unsigned int percentage )
{
    sizeMarker = sizeMarker * (float)percentage / (float)100;

    for( int i = 0; i < texturesVinculadas->size(); i++ )
    {
        if( texturesVinculadas->at( i )->name == textureName )
        {
            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, texturesVinculadas->at( i )->id );
            glColor3f( 1, 1, 1 );
            glEnable( GL_LIGHTING );
            glBegin( GL_QUADS );

                glNormal3f( 0.0f, 0.0f, 1.0f ); // Techo
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, sizeMarker );

                glNormal3f( 0.0f, 0.0f,-1.0f ); // Piso
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );

                glNormal3f( 0.0f, 1.0f, 0.0f ); // Trasera
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );

                glNormal3f( 0.0f,-1.0f, 0.0f ); // Frontal
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, sizeMarker );

                glNormal3f( 1.0f, 0.0f, 0.0f ); // Derecha
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2, -sizeMarker/2, sizeMarker );

                glNormal3f( -1.0f, 0.0f, 0.0f ); // Izquierda
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2, -sizeMarker/2, sizeMarker );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, sizeMarker );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f(-sizeMarker/2,  sizeMarker/2, 0 );

            glEnd();
            glDisable( GL_LIGHTING );
            glDisable( GL_TEXTURE_2D );
        }
    }
}

void Scene::drawModel( QString modelName, int percentage )
{
    float scale = percentage / ( float )1000;
    for ( int i = 0 ; i < models->size(); i++ )
    {
        if ( models->at( i )->name == modelName )
        {
            if( !models->at( i )->totalFaces ) return;

            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, models->at( i )->textureId );
            glScalef( scale, scale, -scale );
            glEnableClientState( GL_VERTEX_ARRAY );
            glEnableClientState( GL_NORMAL_ARRAY );
            glEnableClientState( GL_TEXTURE_COORD_ARRAY );

                glBindBuffer( GL_ARRAY_BUFFER, models->at( i )->normalVBO );
                glNormalPointer( GL_FLOAT, 0, NULL );
                glBindBuffer( GL_ARRAY_BUFFER, models->at( i )->texCoordVBO );
                glTexCoordPointer( 2, GL_FLOAT, 0, NULL );
                glBindBuffer( GL_ARRAY_BUFFER, models->at( i )->vertexVBO );
                glVertexPointer( 3, GL_FLOAT, 0, NULL );
                glDrawArrays( GL_TRIANGLES, 0, models->at( i )->totalFaces * 3 );

            glDisableClientState( GL_VERTEX_ARRAY );
            glDisableClientState( GL_NORMAL_ARRAY );
            glDisableClientState( GL_TEXTURE_COORD_ARRAY );

            glDisable( GL_TEXTURE_2D );
        }
    }
}

void Scene::drawVideo( QString videoName, float sizeMarker, unsigned int percentage, int volume )
{
    sizeMarker = sizeMarker * (float)percentage / (float)100;

    for ( int i = 0 ; i < videos->size(); i++ )
    {
        if ( videos->at( i )->name == videoName )
        {
            videos->at( i )->player->play();
            videos->at( i )->player->setVolume( volume );

            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, videos->at( i )->grabber->textureId );
            glColor3f( 1, 1, 1 );
            glBegin( GL_QUADS );

                glNormal3f( 0.0f, 0.0f,-1.0f);
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2*( 16 / ( float )9 ), -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2*( 16 / ( float )9 ),  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2*( 16 / ( float )9 ),  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2*( 16 / ( float )9 ), -sizeMarker/2, 0 );

            glEnd();
            glDisable( GL_TEXTURE_2D);
        }
    }
}

void Scene::drawVideoVinculado( QString videoName, float sizeMarker, unsigned int percentage, int volume )
{
    sizeMarker = sizeMarker * (float)percentage / (float)100;

    for ( int i = 0 ; i < videosVinculados->size(); i++ )
    {
        if ( videosVinculados->at( i )->name == videoName )
        {
            videosVinculados->at( i )->player->play();
            videosVinculados->at( i )->player->setVolume( volume );

            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, videosVinculados->at( i )->grabber->textureId );
            glColor3f( 1, 1, 1 );
            glBegin( GL_QUADS );

                glNormal3f( 0.0f, 0.0f,-1.0f);
                glTexCoord2f( 1.0f, 0.0f ); glVertex3f(-sizeMarker/2*( 16 / ( float )9 ), -sizeMarker/2, 0 );
                glTexCoord2f( 1.0f, 1.0f ); glVertex3f(-sizeMarker/2*( 16 / ( float )9 ),  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 1.0f ); glVertex3f( sizeMarker/2*( 16 / ( float )9 ),  sizeMarker/2, 0 );
                glTexCoord2f( 0.0f, 0.0f ); glVertex3f( sizeMarker/2*( 16 / ( float )9 ), -sizeMarker/2, 0 );

            glEnd();
            glDisable( GL_TEXTURE_2D);
        }
    }
}

void Scene::decreaseVideosVolume()
{
    for ( int i = 0 ; i < videos->size(); i++ )

        if ( videos->at( i )->player->state() == QMediaPlayer::PlayingState )  {
//            emit message( "<div style=\"color:red;\">Marcador no detectado, el video se pausará</div>" );
            videos->at( i )->player->setVolume( videos->at( i )->player->volume() - 1 );

            if( videos->at( i )->player->volume() <= 0 )
            {
//                emit message( "Video pausado" );
                videos->at( i )->player->pause();
            }
        }
}

void Scene::decreaseVideosVolumeVinculados()
{
    for ( int i = 0 ; i < videosVinculados->size(); i++ )

        if ( videosVinculados->at( i )->player->state() == QMediaPlayer::PlayingState )  {
//            emit message( "<div style=\"color:red;\">Marcador no detectado, el video se pausará</div>" );
            videosVinculados->at( i )->player->setVolume( videosVinculados->at( i )->player->volume() - 1 );

            if( videosVinculados->at( i )->player->volume() <= 0 )
            {
//                emit message( "Video pausado" );
                videosVinculados->at( i )->player->pause();
            }
        }
}

void Scene::slot_updateScene()  {
    videoCapture->operator >>( textures->operator []( 0 )->mat );

    process( textures->operator []( 0 )->mat );

    textures->operator []( 0 )->generateFromMat();
    this->updateGL();
}

void Scene::slot_vincular( int marker_id, QString recurso, QString formatoCaja )
{
    int ok = 0;
    if ( QFileInfo(recurso).suffix() == "mp4" )
        ok = Database::getInstance()->saveVinculo( marker_id, recurso, "n" );  // No se permite video en formato caja
    else
        ok = Database::getInstance()->saveVinculo( marker_id, recurso, formatoCaja );

    if ( ! ok )
        QMessageBox::information(this, "No se pudo vincular", "Intente de nuevo");

}

void Scene::slot_cambiarCamara(int nCamera)
{
    videoCapture->release();
    videoCapture->open(nCamera);
}



