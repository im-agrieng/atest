/***************************************************************************
                          androidplatformutilities.cpp  -  utilities for SIGPAC-Go

                            -------------------
            begin                : February 2016
            copyright            : (C) 2016-2025 by Matthias Kuhn
            email               : matthias@opengis.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "androidplatformutilities.h"
#include "androidprojectsource.h"
#include "androidresourcesource.h"
#include "androidviewstatus.h"
#include "appinterface.h"
#include "fileutils.h"
#include "sigpacgo.h"
#include "sigpacgo_android.h"
#include "sigpacgocloudconnection.h"

#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>

inline QJniObject qtAndroidContext()
{
  return QJniObject( QCoreApplication::instance()->nativeInterface<QNativeInterface::QAndroidApplication>()->context() );
}

inline void runOnAndroidMainThread( const std::function<void()> &runnable )
{
  QCoreApplication::instance()->nativeInterface<QNativeInterface::QAndroidApplication>()->runOnAndroidMainThread( [runnable]() {
    runnable();
    return QVariant();
  } );
}

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMimeDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <qgsfileutils.h>

#include <android/bitmap.h>
#include <android/log.h>
#include <jni.h>

const char *const applicationName = "SIGPAC-Go";

#define GLUE_HELPER( u, v, w, x, y, z ) u##v##w##x##y##z
#define JNI_FUNCTION_NAME( package_name, class_name, function_name ) GLUE_HELPER( Java_ch_opengis_, package_name, _, class_name, _, function_name )

AndroidPlatformUtilities::AndroidPlatformUtilities()
  : mActivity( qtAndroidContext() )
  , mSystemGenericDataLocation( QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ) + QStringLiteral( "/share" ) )
{
}

PlatformUtilities::Capabilities AndroidPlatformUtilities::capabilities() const
{
  PlatformUtilities::Capabilities capabilities = Capabilities() | NativeCamera | AdjustBrightness | CustomImport | CustomExport | CustomSend | FilePicker | VolumeKeys | UpdateProjectFromArchive;
#ifdef WITH_SENTRY
  capabilities |= SentryFramework;
#endif
  return capabilities;
}

void AndroidPlatformUtilities::afterUpdate()
{
  // Copy data away from the virtual path `assets:/` to a path accessible also for non-qt-based libs

  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject messageJni = QJniObject::fromString( QObject::tr( "Please wait while SIGPAC-Go installation finalizes." ) );
        activity.callMethod<void>( "showBlockingProgressDialog", "(Ljava/lang/String;)V", messageJni.object<jstring>() );
      }
    } );
  }

  FileUtils::copyRecursively( QStringLiteral( "assets:/" ), mSystemGenericDataLocation );

  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "dismissBlockingProgressDialog" );
      }
    } );
  }
}

QString AndroidPlatformUtilities::systemSharedDataLocation() const
{
  return mSystemGenericDataLocation;
}

bool AndroidPlatformUtilities::hasQgsProject() const
{
  return !getIntentExtra( "QGS_PROJECT" ).isEmpty();
}

void AndroidPlatformUtilities::loadQgsProject() const
{
  if ( mActivity.isValid() && hasQgsProject() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "processProjectIntent" );
      }
    } );
  }
}

QStringList AndroidPlatformUtilities::appDataDirs() const
{
  const QString dataDirs = getIntentExtra( "SIGPACGO_APP_DATA_DIRS" );
  return ( !dataDirs.isEmpty() ? dataDirs.split( "--;--" ) : QStringList() );
}

QString AndroidPlatformUtilities::applicationDirectory() const
{
  if ( mActivity.isValid() )
  {
    QJniObject rootDirs = mActivity.callObjectMethod<jstring>( "getApplicationDirectory" );
    if ( rootDirs.isValid() )
    {
      return rootDirs.toString();
    }
  }

  return QString();
}

QStringList AndroidPlatformUtilities::additionalApplicationDirectories() const
{
  if ( mActivity.isValid() )
  {
    QJniObject rootDirs = mActivity.callObjectMethod<jstring>( "getAdditionalApplicationDirectories" );
    if ( rootDirs.isValid() )
    {
      return rootDirs.toString().split( "--;--" );
    }
  }

  return QStringList();
}

QStringList AndroidPlatformUtilities::rootDirectories() const
{
  if ( mActivity.isValid() )
  {
    QJniObject rootDirs = mActivity.callObjectMethod<jstring>( "getRootDirectories" );
    if ( rootDirs.isValid() )
    {
      return rootDirs.toString().split( "--;--" );
    }
  }

  return QStringList();
}
void AndroidPlatformUtilities::importProjectFolder() const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "triggerImportProjectFolder" );
      }
    } );
  }
}

void AndroidPlatformUtilities::importProjectArchive() const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "triggerImportProjectArchive" );
      }
    } );
  }
}

void AndroidPlatformUtilities::importDatasets() const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "triggerImportDatasets" );
      }
    } );
  }
}

void AndroidPlatformUtilities::updateProjectFromArchive( const QString &projectPath ) const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [projectPath] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject projectPathJni = QJniObject::fromString( projectPath );
        activity.callMethod<void>( "triggerUpdateProjectFromArchive", "(Ljava/lang/String;)V", projectPathJni.object<jstring>() );
      }
    } );
  }
}

void AndroidPlatformUtilities::sendDatasetTo( const QString &path ) const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [path] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QStringList paths = QStringList() << path;
        const QSet<QString> files = QgsFileUtils::sidecarFilesForPath( path );
        for ( const QString &file : files )
        {
          paths << file;
        }
        QJniObject pathsJni = QJniObject::fromString( paths.join( "--;--" ) );
        activity.callMethod<void>( "sendDatasetTo", "(Ljava/lang/String;)V", pathsJni.object<jstring>() );
      }
    } );
  }
}

void AndroidPlatformUtilities::exportDatasetTo( const QString &path ) const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [path] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QStringList paths = QStringList() << path;
        const QSet<QString> files = QgsFileUtils::sidecarFilesForPath( path );
        for ( const QString &file : files )
        {
          paths << file;
        }
        QJniObject pathsJni = QJniObject::fromString( paths.join( "--;--" ) );
        activity.callMethod<void>( "exportToFolder", "(Ljava/lang/String;)V", pathsJni.object<jstring>() );
      }
    } );
  }
}

void AndroidPlatformUtilities::removeDataset( const QString &path ) const
{
  bool allowed = false;
  const QStringList allowedDirectories = QStringList() << applicationDirectory() << additionalApplicationDirectories();
  for ( const QString &directory : allowedDirectories )
  {
    if ( path.startsWith( directory ) )
    {
      allowed = true;
      break;
    }
  }
  if ( allowed )
  {
    if ( mActivity.isValid() )
    {
      runOnAndroidMainThread( [path] {
        auto activity = qtAndroidContext();
        if ( activity.isValid() )
        {
          QJniObject pathJni = QJniObject::fromString( path );
          activity.callMethod<void>( "removeDataset", "(Ljava/lang/String;)V", pathJni.object<jstring>() );
        }
      } );
    }
  }
}

void AndroidPlatformUtilities::exportFolderTo( const QString &path ) const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [path] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject pathJni = QJniObject::fromString( path );
        activity.callMethod<void>( "exportToFolder", "(Ljava/lang/String;)V", pathJni.object<jstring>() );
      }
    } );
  }
}

void AndroidPlatformUtilities::sendCompressedFolderTo( const QString &path ) const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [path] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject pathJni = QJniObject::fromString( path );
        activity.callMethod<void>( "sendCompressedFolderTo", "(Ljava/lang/String;)V", pathJni.object<jstring>() );
      }
    } );
  }
}
void AndroidPlatformUtilities::removeFolder( const QString &path ) const
{
  bool allowed = false;
  const QStringList allowedDirectories = QStringList() << applicationDirectory() << additionalApplicationDirectories();
  for ( const QString &directory : allowedDirectories )
  {
    if ( path.startsWith( directory ) )
    {
      allowed = true;
      break;
    }
  }
  if ( allowed )
  {
    if ( mActivity.isValid() )
    {
      runOnAndroidMainThread( [path] {
        auto activity = qtAndroidContext();
        if ( activity.isValid() )
        {
          QJniObject pathJni = QJniObject::fromString( path );
          activity.callMethod<void>( "removeProjectFolder", "(Ljava/lang/String;)V", pathJni.object<jstring>() );
        }
      } );
    }
  }
}

ResourceSource *AndroidPlatformUtilities::processCameraActivity( const QString &prefix, const QString &filePath, const QString &suffix, bool isVideo, QObject *parent )
{
  if ( !checkCameraPermissions() )
    return nullptr;

  const QFileInfo destinationInfo( prefix + filePath );
  const QDir prefixDir( prefix );
  prefixDir.mkpath( destinationInfo.absolutePath() );

  AndroidResourceSource *resourceSource = nullptr;
  if ( mActivity.isValid() )
  {
    resourceSource = new AndroidResourceSource( prefix, parent );

    runOnAndroidMainThread( [prefix, filePath, suffix, isVideo] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject prefixJni = QJniObject::fromString( prefix );
        QJniObject filePathJni = QJniObject::fromString( filePath );
        QJniObject suffixJni = QJniObject::fromString( suffix );

        QSettings().setValue( QStringLiteral( "SIGPAC-Go/nativeCameraLaunched" ), true );
        activity.callMethod<void>( "getCameraResource", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V",
                                  prefixJni.object<jstring>(),
                                  filePathJni.object<jstring>(),
                                  suffixJni.object<jstring>(),
                                  isVideo );
      }
    } );
  }
  return resourceSource;
}

ResourceSource *AndroidPlatformUtilities::getCameraPicture( const QString &prefix, const QString &pictureFilePath, const QString &suffix, QObject *parent )
{
  return processCameraActivity( prefix, pictureFilePath, suffix, false, parent );
}

ResourceSource *AndroidPlatformUtilities::getCameraVideo( const QString &prefix, const QString &videoFilePath, const QString &suffix, QObject *parent )
{
  return processCameraActivity( prefix, videoFilePath, suffix, true, parent );
}

ResourceSource *AndroidPlatformUtilities::processGalleryActivity( const QString &prefix, const QString &filePath, const QString &mimeType, QObject *parent )
{
  const QFileInfo destinationInfo( prefix + filePath );
  const QDir prefixDir( prefix );
  prefixDir.mkpath( destinationInfo.absolutePath() );

  AndroidResourceSource *resourceSource = nullptr;
  if ( mActivity.isValid() )
  {
    resourceSource = new AndroidResourceSource( prefix, parent );

    runOnAndroidMainThread( [prefix, filePath, mimeType] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject prefixJni = QJniObject::fromString( prefix );
        QJniObject filePathJni = QJniObject::fromString( filePath );
        QJniObject mimeTypeJni = QJniObject::fromString( mimeType );

        QSettings().setValue( QStringLiteral( "SIGPAC-Go/nativeCameraLaunched" ), true );
        activity.callMethod<void>( "getGalleryResource", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                  prefixJni.object<jstring>(),
                                  filePathJni.object<jstring>(),
                                  mimeTypeJni.object<jstring>() );
      }
    } );
  }
  return resourceSource;
}

ResourceSource *AndroidPlatformUtilities::getGalleryPicture( const QString &prefix, const QString &pictureFilePath, QObject *parent )
{
  return processGalleryActivity( prefix, pictureFilePath, QStringLiteral( "image/*" ), parent );
}

ResourceSource *AndroidPlatformUtilities::getGalleryVideo( const QString &prefix, const QString &videoFilePath, QObject *parent )
{
  return processGalleryActivity( prefix, videoFilePath, QStringLiteral( "video/*" ), parent );
}

ResourceSource *AndroidPlatformUtilities::getFile( const QString &prefix, const QString &filePath, FileType fileType, QObject *parent )
{
  const QFileInfo destinationInfo( prefix + filePath );
  const QDir prefixDir( prefix );
  prefixDir.mkpath( destinationInfo.absolutePath() );
    QString mimeType;
  switch ( fileType )
  {
    case AudioFiles:
      mimeType = "audio/*";
      break;
    case AllFiles:
    default:
      mimeType = "*/*";
      break;
  }

  AndroidResourceSource *resourceSource = nullptr;
  if ( mActivity.isValid() )
  {
    resourceSource = new AndroidResourceSource( prefix, parent );

    runOnAndroidMainThread( [prefix, filePath, mimeType] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject prefixJni = QJniObject::fromString( prefix );
        QJniObject filePathJni = QJniObject::fromString( filePath );
        QJniObject mimeTypeJni = QJniObject::fromString( mimeType );

        QSettings().setValue( QStringLiteral( "SIGPAC-Go/nativeCameraLaunched" ), true );
        activity.callMethod<void>( "getFilePickerResource", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                  prefixJni.object<jstring>(),
                                  filePathJni.object<jstring>(),
                                  mimeTypeJni.object<jstring>() );
      }
    } );
  }
  return resourceSource;
}

ViewStatus *AndroidPlatformUtilities::open( const QString &filePath, bool isEditing, QObject *parent )
{
  if ( QFileInfo( filePath ).isDir() )
    return nullptr;

  QMimeDatabase db;
  const QString mimeType = db.mimeTypeForFile( filePath ).name();

  AndroidViewStatus *viewStatus = nullptr;
  if ( mActivity.isValid() )
  {
    if ( parent )
    {
      viewStatus = new AndroidViewStatus( parent );
    }

    runOnAndroidMainThread( [filePath, mimeType, isEditing] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject filePathJni = QJniObject::fromString( filePath );
        QJniObject mimeTypeJni = QJniObject::fromString( mimeType );

        QSettings().setValue( QStringLiteral( "SIGPAC-Go/nativeCameraLaunched" ), true );
        activity.callMethod<void>( "openResource", "(Ljava/lang/String;Ljava/lang/String;Z)V",
                                  filePathJni.object<jstring>(),
                                  mimeTypeJni.object<jstring>(),
                                  isEditing );
      }
    } );
  }
  return viewStatus;
}

void AndroidPlatformUtilities::requestStoragePermission() const
{
  if ( !QSettings().value( QStringLiteral( "SIGPAC-Go/storagePermissionChecked" ), false ).toBool() )
  {
    QStringList permissions;
    permissions << "android.permission.READ_EXTERNAL_STORAGE"
                << "android.permission.WRITE_EXTERNAL_STORAGE"
                << "android.permission.ACCESS_MEDIA_LOCATION";

    checkAndAcquirePermissions( permissions, true );
    QSettings().setValue( QStringLiteral( "SIGPAC-Go/storagePermissionChecked" ), true );
  }
}

bool AndroidPlatformUtilities::checkPositioningPermissions() const
{
  // First check for coarse permissions. If the user configured SIGPAC-Go to only get coarse permissions
  // it's their wish and we just let it be.
  auto r = QtAndroidPrivate::checkPermission( "android.permission.ACCESS_COARSE_LOCATION" ).result();
  if ( r == QtAndroidPrivate::Denied )
  {
    return checkAndAcquirePermissions( { QStringLiteral( "android.permission.ACCESS_FINE_LOCATION" ) } );
  }
  return true;
}

bool AndroidPlatformUtilities::checkCameraPermissions() const
{
  return checkAndAcquirePermissions( { QStringLiteral( "android.permission.CAMERA" ) } );
}

bool AndroidPlatformUtilities::checkMicrophonePermissions() const
{
  return checkAndAcquirePermissions( { QStringLiteral( "android.permission.RECORD_AUDIO" ) } );
}

bool AndroidPlatformUtilities::checkAndAcquirePermissions( QStringList permissions, bool forceAsk ) const
{
  if ( !forceAsk )
  {
    permissions.erase( std::remove_if( permissions.begin(), permissions.end(),
                                      []( const QString &permission ) {
                                        auto r = QtAndroidPrivate::checkPermission( permission ).result();
                                        return r != QtAndroidPrivate::Denied;
                                      } ),
                      permissions.end() );
  }

  if ( !permissions.isEmpty() )
  {
    for ( const QString &permission : permissions )
    {
      auto r = QtAndroidPrivate::requestPermission( permission ).result();
      if ( r == QtAndroidPrivate::Denied )
      {
        if ( !forceAsk )
        {
          return false;
        }
      }
    }
  }

  return true;
}

void AndroidPlatformUtilities::setScreenLockPermission( const bool allowLock )
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [allowLock] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        QJniObject window = activity.callObjectMethod( "getWindow", "()Landroid/view/Window;" );

        if ( window.isValid() )
        {
          const int FLAG_KEEP_SCREEN_ON = 128;
          if ( !allowLock )
          {
            window.callMethod<void>( "addFlags", "(I)V", FLAG_KEEP_SCREEN_ON );
          }
          else
          {
            window.callMethod<void>( "clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON );
          }
        }
      }

      QJniEnvironment env;
      if ( env->ExceptionCheck() )
      {
        env->ExceptionClear();
      }
    } );
  }
}

void AndroidPlatformUtilities::dimBrightness()
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "dimBrightness" );
      }
    } );
  }
}

void AndroidPlatformUtilities::restoreBrightness()
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "restoreBrightness" );
      }
    } );
  }
}

void AndroidPlatformUtilities::setHandleVolumeKeys( const bool handle )
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [handle] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( handle ? "takeVolumeKeys" : "releaseVolumeKeys" );
      }
    } );
  }
}

void AndroidPlatformUtilities::uploadPendingAttachments( SIGPACGoCloudConnection *connection ) const
{
  // Request notification permission
  checkAndAcquirePermissions( { QStringLiteral( "android.permission.POST_NOTIFICATIONS" ) } );

  QTimer::singleShot( 500, [connection]() {
    if ( connection )
    {
      qInfo() << "Launching SIGPAC-Go Cloud service...";
      QJniObject::callStaticMethod<void>( "ch/opengis/" APP_PACKAGE_NAME "/SIGPACGoCloudService",
                                         "startSIGPACGoCloudService",
                                         "(Landroid/content/Context;)V",
                                         qtAndroidContext().object() );
    }
  } );
}
QVariantMap AndroidPlatformUtilities::sceneMargins( QQuickWindow *window ) const
{
  Q_UNUSED( window )

  double statusBarMargin = std::abs( static_cast<double>( mActivity.callMethod<jdouble>( "statusBarMargin" ) ) );
  double navigationBarMargin = std::abs( static_cast<double>( mActivity.callMethod<jdouble>( "navigationBarMargin" ) ) );

  statusBarMargin /= QGuiApplication::primaryScreen()->devicePixelRatio();
  navigationBarMargin /= QGuiApplication::primaryScreen()->devicePixelRatio();

  QVariantMap margins;
  margins[QLatin1String( "top" )] = statusBarMargin;
  margins[QLatin1String( "right" )] = 0.0;
  margins[QLatin1String( "bottom" )] = navigationBarMargin;
  margins[QLatin1String( "left" )] = 0.0;
  return margins;
}

bool AndroidPlatformUtilities::isSystemDarkTheme() const
{
  if ( mActivity.isValid() )
  {
    bool isDarkTheme = mActivity.callMethod<jboolean>( "isDarkTheme" ) == JNI_TRUE;
    return isDarkTheme;
  }
  return false;
}

void AndroidPlatformUtilities::vibrate( int milliseconds ) const
{
  if ( mActivity.isValid() )
  {
    runOnAndroidMainThread( [milliseconds] {
      auto activity = qtAndroidContext();
      if ( activity.isValid() )
      {
        activity.callMethod<void>( "vibrate", "(I)V", milliseconds );
      }
    } );
  }
}

void AndroidPlatformUtilities::requestBackgroundPositioningPermissions()
{
  checkAndAcquirePermissions( { QStringLiteral( "android.permission.ACCESS_BACKGROUND_LOCATION" ) } );
}

void AndroidPlatformUtilities::startPositioningService() const
{
  // Request notification permission
  checkAndAcquirePermissions( { QStringLiteral( "android.permission.POST_NOTIFICATIONS" ) } );

  qInfo() << "Launching SIGPAC-Go positioning service...";
  QJniObject::callStaticMethod<void>( "ch/opengis/" APP_PACKAGE_NAME "/SIGPACGoPositioningService",
                                     "startSIGPACGoPositioningService",
                                     "(Landroid/content/Context;)V",
                                     qtAndroidContext().object() );
}

void AndroidPlatformUtilities::stopPositioningService() const
{
  qInfo() << "Terminating SIGPAC-Go positioning service...";
  QJniObject::callStaticMethod<void>( "ch/opengis/" APP_PACKAGE_NAME "/SIGPACGoPositioningService",
                                     "stopSIGPACGoPositioningService",
                                     "(Landroid/content/Context;)V",
                                     qtAndroidContext().object() );
}

#ifdef __cplusplus
extern "C" {
#endif

// SIGPACGoActivity class functions
JNIEXPORT void JNICALL JNI_FUNCTION_NAME( APP_PACKAGE_JNI_NAME, SIGPACGoActivity, openProject )( JNIEnv *env, jobject obj, jstring path )
{
  if ( AppInterface::instance() )
  {
    const char *pathStr = env->GetStringUTFChars( path, NULL );
    AppInterface::instance()->loadFile( QString( pathStr ) );
    env->ReleaseStringUTFChars( path, pathStr );
  }
  return;
}

JNIEXPORT void JNICALL JNI_FUNCTION_NAME( APP_PACKAGE_JNI_NAME, SIGPACGoActivity, openPath )( JNIEnv *env, jobject obj, jstring path )
{
  if ( AppInterface::instance() )
  {
    const char *pathStr = env->GetStringUTFChars( path, NULL );
    emit AppInterface::instance()->openPath( QString( pathStr ) );
    env->ReleaseStringUTFChars( path, pathStr );
  }
  return;
}

#define ANDROID_VOLUME_DOWN 25
#define ANDROID_VOLUME_UP 24

JNIEXPORT void JNICALL JNI_FUNCTION_NAME( APP_PACKAGE_JNI_NAME, SIGPACGoActivity, volumeKeyDown )( JNIEnv *env, jobject obj, int volumeKeyCode )
{
  if ( AppInterface::instance() )
  {
    emit AppInterface::instance()->volumeKeyDown( volumeKeyCode == ANDROID_VOLUME_DOWN ? Qt::Key_VolumeDown : Qt::Key_VolumeUp );
  }
  return;
}

JNIEXPORT void JNICALL JNI_FUNCTION_NAME( APP_PACKAGE_JNI_NAME, SIGPACGoActivity, volumeKeyUp )( JNIEnv *env, jobject obj, int volumeKeyCode )
{
  if ( AppInterface::instance() )
  {
    emit AppInterface::instance()->volumeKeyUp( volumeKeyCode == ANDROID_VOLUME_DOWN ? Qt::Key_VolumeDown : Qt::Key_VolumeUp );
  }
  return;
}

#ifdef __cplusplus
}
#endif

  