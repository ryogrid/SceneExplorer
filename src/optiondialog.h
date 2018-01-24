#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H


#include "ui_option.h"

#include "globals.h"

class OptionDialog:public QDialog
{
    Q_OBJECT

private:
    Ui::Option ui;

public:
    OptionDialog(QWidget* parent = nullptr);

    ImageCacheType imagecache_;
    int maxgd_;
    int maxff_;
    QString dbdir_;
    bool openlastdoc_;
    QString ffprobe_, ffmpeg_;

protected:
    void showEvent(QShowEvent *) override;
private slots:
    void on_buttonBox_accepted();
    void on_tbDBDir_clicked();
    void on_tbffprobe_clicked();
    void on_tbffmpeg_clicked();
};

#endif // OPTIONDIALOG_H
