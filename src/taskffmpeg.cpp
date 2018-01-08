#include <QThread>
#include <QProcess>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDir>

#include "globals.h"
#include "consts.h"

#include "taskffmpeg.h"

TaskFFmpeg::TaskFFmpeg(int id,const QString& file)
{
    id_=id;
    movieFile_=file;

    progress_ = Uninitialized;
    // emit sayBorn(id,file);
}
TaskFFmpeg::~TaskFFmpeg()
{
    emit sayDead(id_);
}
bool getProbe(const QString& file,
              double& outDuration,
              QString& outFormat,

              QString& outVideoCodec,
              QString& outAudioCodec,

              int& outVWidth,
              int& outVHeight)
{
    QProcess process;
    process.setProgram("C:\\LegacyPrograms\\ffmpeg\\bin\\ffprobe.exe");
    process.setArguments( QStringList() <<
                          "-v" <<
                          "quiet" <<
                          "-hide_banner" <<
                          "-show_format" <<
                          "-show_streams" <<
                          "-print_format" <<
                          "json" <<
                          file
                          );

    process.start(QProcess::ReadOnly);
    if(!process.waitForStarted())
        return false;

    if(!process.waitForFinished())
        return false;

    qDebug() << "exitCode()=" << process.exitCode();
    if(0 != process.exitCode())
            return false;

    QByteArray baOut = process.readAllStandardOutput();
    QByteArray baErr=process.readAllStandardError();


    QJsonDocument jd = QJsonDocument::fromJson(baOut);
    if(jd.isNull())
        return false;

    QJsonObject jo = jd.object();
    if(jo.isEmpty())
        return false;

    // format and duration
    {
        QJsonValue format = jo.value(QString("format"));
        QJsonObject item = format.toObject();

        QJsonValue format_name = item["format_name"];
        if(
                format_name.toString()=="tty" ||
                format_name.toString()=="image2"
          )
        {
            return false;
        }
        outFormat = format_name.toString();

        QJsonValue jDuration = item["duration"];

        if(!jDuration.isString())
            return false;

        QString ds = jDuration.toString();

        bool ok;
        outDuration = ds.toDouble(&ok);
        if(!ok)
            return false;
    }

    // stream
    outVideoCodec.clear();
    outAudioCodec.clear();
    {
        QJsonValue streams = jo.value(QString("streams"));
        for(const QJsonValue& value : streams.toArray())
        {
            QJsonObject item = value.toObject();

            QString ctype = item["codec_type"].toString();
            if(outVideoCodec.isEmpty() && ctype=="video")
            {
                outVideoCodec = item["codec_name"].toString();
                outVWidth = item["coded_width"].toInt();
                outVHeight = item["coded_height"].toInt();
            }
            if(outAudioCodec.isEmpty() && ctype=="audio")
            {
                outAudioCodec = item["codec_name"].toString();
            }

            if(!outVideoCodec.isEmpty() && !outAudioCodec.isEmpty())
                break;
        }
    }
    return true;
}

void TaskFFmpeg::run()
{
    if(gStop)
        return;
    while(gPaused)
        QThread::sleep(5);
    if(gStop)
        return;

    progress_ = Progressing;
    emit sayHello(id_, movieFile_);
    if(!run2())
        emit sayNo(id_, movieFile_);
    progress_ = Finished;
}
bool TaskFFmpeg::run2()
{
    double duration;
    QString format;
    QString vcodec,acodec;
    int vWidth,vHeight;
    if(!getProbe(movieFile_,
                 duration,
                 format,
                 vcodec,
                 acodec,
                 vWidth,vHeight))
    {
        return false;
    }

    QString strWidthHeight;
    strWidthHeight.append(QString::number(Consts::THUMB_WIDTH));
    strWidthHeight.append("x");
    strWidthHeight.append(QString::number(Consts::THUMB_HEIGHT));

    QString thumbfile = QUuid::createUuid().toString();
    thumbfile = thumbfile.remove(L'{');
    thumbfile = thumbfile.remove(L'}');
    QStringList emitFiles;
    for(int i=1 ; i <=5 ;++i)
    {
        QString filename=thumbfile;
        filename.append("-");
        filename.append(QString::number(i));
        filename.append(".png");

        QString actualFile = QString(Consts::FILEPART_THUMBS) + QDir::separator() + filename;

        double timepoint = (((double)i-0.5)*duration/5);
        QStringList qsl;
        qsl.append("-v");
        qsl.append("quiet");  // no log
        qsl.append("-hide_banner");  // as it is
        qsl.append("-n");  // no overwrite
        qsl.append("-ss" );  // seek input
        qsl.append(QString::number(timepoint) );  // seek position
        qsl.append("-i" );  // input file
        qsl.append(movieFile_ );  // input file
        qsl.append("-vf" );  // video filtergraph
        qsl.append("select='eq(pict_type\\,I)'");  // select filter with argument, Select only I-frames:
        qsl.append("-vframes" );
        qsl.append("1");
        qsl.append("-s");
        qsl.append(strWidthHeight);
        qsl.append(actualFile);

        QProcess ffmpeg;
        ffmpeg.setProgram("C:\\LegacyPrograms\\ffmpeg\\bin\\ffmpeg.exe");
        ffmpeg.setArguments(qsl);
        ffmpeg.start(QProcess::ReadOnly);

        if(!ffmpeg.waitForStarted())
            return false;

        if(!ffmpeg.waitForFinished())
            return false;

        if(ffmpeg.exitCode() != 0)
            return false;

//        QByteArray baOut = ffmpeg.readAllStandardOutput();
//        qDebug()<<baOut.data();

//        QByteArray baErr=ffmpeg.readAllStandardError();
//        qDebug() << baErr.data();

        if(i==1)
        {
            if(!QFile(actualFile).exists())
                return false;
        }
        emitFiles.append(filename);
    }

    // emit sayGoodby(id_,emitFiles, Consts::THUMB_WIDTH, Consts::THUMB_HEIGHT, movieFile_, format);
    emit sayGoodby(id_,
                   emitFiles,
                   movieFile_,
                   Consts::THUMB_WIDTH,
                   Consts::THUMB_HEIGHT,
                   duration,
                   format,
                   vcodec,acodec,
                   vWidth,vHeight
                   );
    return true;
}
