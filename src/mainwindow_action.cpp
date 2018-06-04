//SceneExplorer
//Exploring video files by viewer thumbnails
//
//Copyright (C) 2018  Ambiesoft
//
//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <QDesktopServices>
#include <QMessageBox>
#include <QDialog>
#include <QDebug>
#include <QThreadPool>
#include <QFileDialog>
#include <QListWidget>
#include <QInputDialog>
#include <QColor>

#include "consts.h"
#include "globals.h"
#include "waitcursor.h"
#include "ffmpeg.h"
#include "taskmodel.h"
#include "tableitemdata.h"

#include "optiondialog.h"

#include "taskgetdir.h"
#include "directoryentry.h"

#include "settings.h"
#include "sql.h"
#include "helper.h"
#include "osd.h"
#include "blockedbool.h"
#include "tagitem.h"

#include "mycontextmenu.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"

#include "aboutdialog.h"

using namespace Consts;

void MainWindow::updateOnOpened(const qint64& id, const QString& movieFile)
{
    if(pDoc_)
    {
        pDoc_->IncrementOpenCountAndLastAccess(id);
        int opencount=0;
        qint64 lastaccess=0;
        pDoc_->GetOpenCountAndLastAccess(id,opencount,lastaccess);

        tableModel_->UpdateOpenCountAndLastAccess(movieFile,opencount,lastaccess);
    }
}
void MainWindow::openVideo(const qint64& id, const QString& movieFile)
{
    if(movieFile.isEmpty()) { Alert(this, TR_NOVIDEO_SELECTED()); return;}

    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(movieFile)))
    {
        Alert(this, tr("failed to launch %1.").arg(movieFile));
        return;
    }
    updateOnOpened(id,movieFile);
}
void MainWindow::openVideoInFolder(const QString& movieFile)
{
    if(movieFile.isEmpty()) { Alert(this, TR_NOVIDEO_SELECTED()); return;}

    QString openfile = movieFile;
    if(!QFileInfo(movieFile).exists())
    {
        openfile = QFileInfo(movieFile).absoluteDir().absolutePath();
    }
    if(!showInGraphicalShell(this, openfile))
        Alert(this, tr("Failed to open folder."));
}
void MainWindow::on_tableView_doubleClicked(const QModelIndex &index)
{
    QVariant v = proxyModel_->data(index, TableModel::TableRole::MovieFileFull);
    Q_ASSERT(v.isValid());
    Q_ASSERT(!v.toString().isEmpty());

    QVariant vID = proxyModel_->data(index, TableModel::TableRole::ID);
    openVideo(vID.toLongLong(), v.toString());
}


void MainWindow::on_action_Options_triggered()
{
    OptionDialog dlg(this);
    dlg.maxgd_ = optionThreadcountGetDir_;
    dlg.maxff_ = optionThreadcountThumbnail_;
    dlg.thumbCount_ = optionThumbCount_;
    dlg.thumbFormat_ = optionThumbFormat_;
    dlg.taskPriority_ = GetTaskPriorityAsInt();
    dlg.mainText_ = tableModel_->GetTitleTextTemplate();
    dlg.subText_ = tableModel_->GetInfoTextTemplate();
    dlg.imagecache_ = tableModel_->GetImageCache();
    bool prevUseCustomDBDir = dlg.useCustomDBDir_ = settings_.valueBool(KEY_USE_CUSTOMDATABASEDIR);
    QString prevDbdir = dlg.dbdir_ = settings_.valueString(KEY_DATABASE_PATH); //QDir::currentPath();
    dlg.limitItems_ = settings_.valueBool(KEY_LIMIT_ITEMS, false);
    dlg.maxRows_ = settings_.valueInt(KEY_LIMIT_NUMBEROFROWS, 1000);
    dlg.openlastdoc_ = settings_.valueBool(KEY_OPEN_LASTOPENEDDOCUMENT, true);
    dlg.ffprobe_ = settings_.valueString(KEY_FFPROBE_EXECUTABLE); // FFMpeg::GetFFprobe(settings_);
    dlg.ffmpeg_ = settings_.valueString(KEY_FFMPEG_EXECUTABLE); //FFMpeg::GetFFmpeg(settings_);

    if(QDialog::Accepted != dlg.exec())
        return;

    optionThreadcountGetDir_ = dlg.maxgd_;
    optionThreadcountThumbnail_ = dlg.maxff_;
    optionThumbCount_ = dlg.thumbCount_;
    optionThumbFormat_ = dlg.thumbFormat_;
    SetTaskPriorityAsInt(dlg.taskPriority_);
    tableModel_->SetColumnCountImage(dlg.thumbCount_);
    tableModel_->SetTitleTextTemplate(dlg.mainText_);
    tableModel_->SetInfoTextTemplate(dlg.subText_);
    tableModel_->SetImageCache(dlg.imagecache_);
    settings_.setValue(KEY_LIMIT_ITEMS, dlg.limitItems_);
    settings_.setValue(KEY_LIMIT_NUMBEROFROWS, dlg.maxRows_);
    settings_.setValue(KEY_OPEN_LASTOPENEDDOCUMENT,dlg.openlastdoc_);
    settings_.setValue(KEY_USE_CUSTOMDATABASEDIR, dlg.useCustomDBDir_);
    settings_.setValue(KEY_DATABASE_PATH, dlg.dbdir_);
    FFMpeg::SetFFprobe(settings_, dlg.ffprobe_);
    FFMpeg::SetFFmpeg(settings_, dlg.ffmpeg_);

    CreateLimitManager();

    if( (prevUseCustomDBDir != dlg.useCustomDBDir_) || (prevDbdir != dlg.dbdir_) )
    {
        askRebootClose();
    }
}
void MainWindow::onUserDirectoryTriggered()
{
    QAction* action = (QAction*)sender();
    if(!action)
        return;

    int index = action->data().toInt();
    DirectoryItem* di = (DirectoryItem*) ui->directoryWidget->item(index);

    di->setCheckState(di->IsChecked() ? Qt::Unchecked : Qt::Checked);
}

void MainWindow::onMenuFolder_AboutToShow()
{
//    ui->menu_Folder->clear();
//    ui->menu_Folder->addAction(ui->action_Add_Folder);
//    ui->menu_Folder->addSeparator();

    int startPos = 2;
    QList<QAction*> actions = ui->menu_Folder->actions();
    for(int i=startPos ; i < actions.count(); ++i)
    {
        QAction* pA = actions[i];
        ui->menu_Folder->removeAction(pA);
        delete pA;
    }

    bool bAdded = false;
    for(int i=0 ; i < ui->directoryWidget->count(); ++i)
    {
        DirectoryItem* di = (DirectoryItem*)ui->directoryWidget->item(i);
        if(di->IsAllItem() || di->IsMissingItem())
            continue;
        bAdded = true;
        QString dir = di->text();
        QAction* action = new QAction(dir);
        action->setCheckable(true);
        action->setChecked(di->IsChecked());
        action->setData(i);
        connect(action, &QAction::triggered,
                this, &MainWindow::onUserDirectoryTriggered);
        ui->menu_Folder->addAction(action);
    }

    if(!bAdded)
    {
        QAction* actionNoDir = new QAction(tr("<No Directories>"));
        actionNoDir->setEnabled(false);
        ui->menu_Folder->addAction(actionNoDir);
    }
}

void MainWindow::OnUserTagTriggered()
{
    QAction* action = (QAction*)sender();
    if(!action)
        return;

    int index = action->data().toInt();
    TagItem* ti = (TagItem*) ui->listTag->item(index);


    ti->setCheckState(ti->IsChecked() ? Qt::Unchecked : Qt::Checked);
}
void MainWindow::onMenuTag_AboutToShow()
{
    int startPos = 2;
    QList<QAction*> actions = ui->menu_Tag->actions();
    for(int i=startPos ; i < actions.count(); ++i)
    {
        QAction* pA = actions[i];
        ui->menu_Tag->removeAction(pA);
        delete pA;
    }

    bool bAdded = false;
    for(int i=0; i < ui->listTag->count(); ++i)
    {
        TagItem* ti = (TagItem*)ui->listTag->item(i);
        if(ti->IsAllItem())
            continue;
        bAdded=true;
        QString text = ti->text();
        QAction* action = new QAction(text);
        action->setCheckable(true);
        action->setChecked(ti->IsChecked());
        action->setData(i);
        connect(action, &QAction::triggered,
                this, &MainWindow::OnUserTagTriggered);
        ui->menu_Tag->addAction(action);
    }

    if(!bAdded)
    {
        QAction* actionNoDir = new QAction(tr("<No Tags>"));
        actionNoDir->setEnabled(false);
        ui->menu_Tag->addAction(actionNoDir);
    }
}
void MainWindow::onMenuTask_AboutToShow()
{
    qDebug() << "gPaused" << gPaused;
    ui->action_Pause->setChecked(gPaused);
}


void MainWindow::onMenuDocking_windows_AboutToShow()
{
    ui->action_DockFolder->setChecked(!ui->dockFolder->isHidden());
    ui->action_DockTask->setChecked(!ui->dockTask->isHidden());
    ui->action_DockOutput->setChecked(!ui->dockOutput->isHidden());
    ui->action_DockTag->setChecked(!ui->dockTag->isHidden());
}
void MainWindow::on_action_DockFolder_triggered()
{
    if(ui->dockFolder->isHidden())
        ui->dockFolder->show();
    else
        ui->dockFolder->hide();
}

void MainWindow::on_action_DockTask_triggered()
{
    if(ui->dockTask->isHidden())
        ui->dockTask->show();
    else
        ui->dockTask->hide();
}

void MainWindow::on_action_DockOutput_triggered()
{
    if(ui->dockOutput->isHidden())
        ui->dockOutput->show();
    else
        ui->dockOutput->hide();
}
void MainWindow::on_action_DockTag_triggered()
{
    if(ui->dockTag->isHidden())
        ui->dockTag->show();
    else
        ui->dockTag->hide();
}
//void MainWindow::on_FavoriteFolder_triggered(bool checked)
//{
//    Q_UNUSED(checked);
//    QAction* qa = (QAction*)QObject::sender();
//    Q_ASSERT(qa);
//    QStringList dirs = settings_.GetFavorite(qa->text());
//    ui->directoryWidget->SetCheck(dirs, true);
//    directoryChangedCommon(true);
//}

//void MainWindow::onMenu_Favorites_AboutToShow()
//{
//    int sepIndex = 2;
//    // TODO
//    QMenu* menu = ui->menu_Favorites;
//    foreach (QAction *action, menu->actions()) {
//         if (action->isSeparator()) {
//             qDebug("this action is a separator");
//         } else if (action->menu()) {
//             qDebug("action: %s", qUtf8Printable(action->text()));
//             qDebug(">>> this action is associated with a submenu, iterating it recursively...");
//             // enumerateMenu(action->menu());
//             qDebug("<<< finished iterating the submenu");
//         } else {
//             qDebug("action: %s", qUtf8Printable(action->text()));
//         }
//     }

//    while(ui->menu_Favorites->actions().count() > sepIndex)
//    {
//        QAction *qa = ui->menu_Favorites->actions()[sepIndex];
//        ui->menu_Favorites->removeAction(qa);
//        QObject::disconnect(qa, &QAction::triggered,
//                            this, &MainWindow::on_FavoriteFolder_triggered);
//        delete qa;
//    }
//    QStringList favs = settings_.GetFavorites();
//    for(int i=0; i < favs.count(); ++i)
//    {
//        QAction* qa = new QAction(this);
//        qa->setText(favs[i]);
//        QObject::connect(qa, &QAction::triggered,
//                         this, &MainWindow::on_FavoriteFolder_triggered);
//        ui->menu_Favorites->addAction(qa);
//    }
//}

void MainWindow::OnRecentDocumentsTriggered(bool checked)
{
    Q_UNUSED(checked);
    QAction* qa = (QAction*)QObject::sender();
    Q_ASSERT(qa);

    QString file = qa->text();
    OpenDocument(file, true);
}
void MainWindow::OnClearRecentItemsTriggered(bool checked)
{
    Q_UNUSED(checked);
    recents_.clear();
    if(pDoc_)
        recents_.append(pDoc_->GetFullName());
}
void MainWindow::onMenu_RecentDocuments_AboutToShow()
{
    QString current = pDoc_ ? pDoc_->GetFullName() : QString();


    while(ui->menu_Recent_documets->actions().count() > 0)
    {
        QAction *qa = ui->menu_Recent_documets->actions()[0];
        ui->menu_Recent_documets->removeAction(qa);
        QObject::disconnect(qa, &QAction::triggered,
                            this, &MainWindow::OnRecentDocumentsTriggered);
        delete qa;
    }

    for(int i=0; i < recents_.count(); ++i)
    {
        QAction* qa = new QAction(this);

        // Text
        qa->setText(recents_[i]);

        // Fi file not exists, disable it
        if(!QFile(recents_[i]).exists())
            qa->setEnabled(false);

        // check Current item
        if(!current.isEmpty() && recents_[i]==current)
        {
            qa->setCheckable(true);
            qa->setChecked(true);
        }
        QObject::connect(qa, &QAction::triggered,
                         this, &MainWindow::OnRecentDocumentsTriggered);
        ui->menu_Recent_documets->addAction(qa);
    }

    ui->menu_Recent_documets->addSeparator();


    // Append Clear Recent
    QAction* qaClearRecent = new QAction(this);
    qaClearRecent->setText(tr("&Clear Recent items"));
    connect(qaClearRecent, &QAction::triggered,
            this, &MainWindow::OnClearRecentItemsTriggered);
    ui->menu_Recent_documets->addAction(qaClearRecent);
}

void MainWindow::on_action_Pause_triggered()
{
    gPaused = !gPaused;

    if(gPaused)
    {
        slPaused_->setText(tr("Paused"));
        slPaused_->show();
    }
    else
    {
        slPaused_->setText(QString());
        slPaused_->hide();
    }
}

void MainWindow::AddUserEntryDirectory_obsolete(
        DirectoryItem::DirectoryItemType itemType,
        const qint64& dirid,
        const QString& cdir,
        bool sel,
        bool check)
{
    QString text;
	bool hasCheckbox = true;
    if(itemType==DirectoryItem::DI_NORMAL)
    {
        QDir di(cdir);

        for(int i=0 ; i < ui->directoryWidget->count(); ++i)
        {
            DirectoryItem* item = (DirectoryItem*)ui->directoryWidget->item(i);
            if(!item->IsNormalItem())
                continue;
            QDir d(item->text());
            if(di == d)
            {
                return;
            }
        }
        text=normalizeDir(cdir);
    }
    else if(itemType==DirectoryItem::DI_ALL)
    {
        text=tr("All");
		hasCheckbox = false;
    }
    else if(itemType==DirectoryItem::DI_MISSING)
    {
        text=tr("Missing");
		hasCheckbox = false;
    }
    else
    {
        Q_ASSERT(false);
        return;
    }
    DirectoryItem* newitem = new DirectoryItem(dirid,
                                               itemType,
                                               text);

	if(hasCheckbox)
		newitem->setFlags(newitem->flags() | Qt::ItemIsUserCheckable);

    newitem->setSelected(sel);
	if(hasCheckbox)
		newitem->setCheckState(check ? Qt::Checked : Qt::Unchecked);



    ui->directoryWidget->addItem(newitem);
}

bool IsSubDir(const QString& parent, const QString& child)
{
    QDir childDir(child);
    do
    {
        if(QDir(parent)==childDir)
            return true;
    } while(childDir.cdUp());

    return false;
}
bool MainWindow::IsAllTagSelected() const
{
    TagItem* ti = (TagItem*)ui->listTag->item(0);
    Q_ASSERT(ti->IsAllItem());

    return ui->listTag->currentItem()==ti || ti->isSelected();
}
bool MainWindow::IsDirSelected(const QString& dir) const
{
    if(!IsAllTagSelected())
        return false;

    if(ui->directoryWidget->IsAllItemSelectedOrChecked())
        return true;

    foreach(const QString& d, lastQueriedDirs_)
    {
        if(IsSubDir(d, dir))
            return true;
    }
    return false;
}

void MainWindow::StartScan(QListWidgetItem* item)
{
    StartScan(item->text());
}


void MainWindow::StartScan(const QString& dir)
{
    QString errString;
    if(!checkFFprobe(errString) || !checkFFmpeg(errString))
    {
        insertLog(TaskKind::App, 0,
                  tr("Failed to launch ffprobe or ffmpeg. (%1)").arg(errString) +
                  " " +
                  tr("Check the option setting."));
        return;
    }

    if(!QDir(dir).exists())
    {
        insertLog(TaskKind::App, 0, tr("Directoy not found. (%1) ").
                  arg(dir));
        return;
    }
    TaskGetDir* pTaskGetDir = new TaskGetDir(
                gLoopId,
                idManager_->Increment(IDKIND_GetDir),
                dir,
                GetTaskPriority());
    pTaskGetDir->setAutoDelete(true);
    QObject::connect(pTaskGetDir, &TaskGetDir::afterGetDir,
                     this, &MainWindow::afterGetDir);
    QObject::connect(pTaskGetDir, &TaskGetDir::finished_GetDir,
                     this, &MainWindow::finished_GetDir);
    getPoolGetDir()->start(pTaskGetDir);

    onTaskStarted();
    insertLog(TaskKind::GetDir, idManager_->Get(IDKIND_GetDir),
              tr("Task registered. %1").arg(dir));
}

void MainWindow::onTaskStarted()
{
    // statusBar()->palette().setColor(QPalette::Background, QColor("palegreen"));

    statusBar()->setStyleSheet("background-color : red; color : white;");
}
void MainWindow::onTaskEnded()
{
    // statusBar()->setPalette(QApplication::palette());
    statusBar()->setStyleSheet(QString());
}
void MainWindow::on_action_Stop_triggered()
{
    gPaused = false;
    WaitCursor wc;

    clearAllPool();
    onTaskEnded();
}

void MainWindow::on_action_Close_triggered()
{
    this->close();
}

void MainWindow::on_action_About_triggered()
{
    AboutDialog dlg(this);
    dlg.exec();

//    QString title = APPNAME_DISPLAY;
//    QString text = APPNAME;
//    text.append(" ");
//    text.append("ver ");
//    text.append(VERSION);
//    text.append("\n");
//    text.append("\n");
//    text.append("copyright 2018 ");
//    text.append(ORGANIZATION);
//    QMessageBox msgbox(this);
//    msgbox.setIcon(QMessageBox::Information);
//    msgbox.setText(text);
//    msgbox.setWindowTitle(title);
//    msgbox.exec();
}

void MainWindow::SortManager::setAction(SORTCOLUMNMY sc,
                                            QAction* pAction)
{
    Q_ASSERT(acs_[sc]==nullptr);
    acs_[sc] = pAction;
}
void MainWindow::SortManager::setToolButton(SORTCOLUMNMY sc,
                                            QToolButton* tb,
                                            const QIcon& iconNormal,
                                            const QIcon& iconRev)
{
	Q_ASSERT(tbs_[sc] == nullptr);
    Q_ASSERT(!iconNormal.isNull());
    Q_ASSERT(!iconRev.isNull());

    tbs_[sc] = tb;
    iconsAscend_[sc] = iconNormal;
    iconsDescend_[sc]=iconRev;
	tb->setText(GetSortColumnName(sc));
}

void MainWindow::SortManager::onSort(SORTCOLUMNMY sc)
{
    if(sort_==sc)
        ascending_[sc] = !ascending_[sc];
	sort_ = sc;
	UpdateButtonText();
}
void MainWindow::SortManager::UpdateButtonText()
{
    Q_STATIC_ASSERT(COUNT_SORTCOLUMN == (sizeof(tbs_) / sizeof(tbs_[0])));
    Q_STATIC_ASSERT(COUNT_SORTCOLUMN == (sizeof(acs_) / sizeof(acs_[0])));

    Q_STATIC_ASSERT(SORT_NONE==0);

    for (size_t i = SORT_NONE+1; i < COUNT_SORTCOLUMN; ++i)
	{
        Q_ASSERT(acs_[i]);
        acs_[i]->setChecked(sort_ == i);

        if(tbs_[i])
        {
            acs_[i]->setIcon(ascending_[i] ? iconsAscend_[i] : iconsDescend_[i]);

            QString text = GetSortColumnName((SORTCOLUMNMY)i);
            if(sort_ == i)
                text += (ascending_[i] ? " ASC" : " DESC");
            tbs_[i]->setText( text );
            tbs_[i]->setChecked(sort_ == i);
            tbs_[i]->setIcon(ascending_[i] ? iconsAscend_[i] : iconsDescend_[i]);
        }
//        else
//        {
//            acs_[i]->setChecked(false);

//            if(tbs_[i])
//            {
//                acs_[i]->setIcon(ascending_[i] ? iconsAscend_[i] : iconsDescend_[i]);

//                tbs_[i]->setText(GetSortColumnName((SORTCOLUMNMY)i));
//                tbs_[i]->setChecked(false);
//                tbs_[i]->setIcon(ascending_[i] ? iconsAscend_[i] : iconsDescend_[i]);
//            }
//        }
	}
}


void MainWindow::onSortCommon(SORTCOLUMNMY sortColumn)
{
	WaitCursor wc;
	sortManager_.onSort(sortColumn);
	if (limitManager_)
		limitManager_->Reset();
    GetSqlAllSetTable(lastQueriedDirs_, lastQueriedIsTagValid_ ? &lastQueriedTaggedIDs_:nullptr);
}
void MainWindow::on_actionSort_by_file_name_triggered()
{
	onSortCommon(SORT_FILENAME);
}
void MainWindow::on_actionSort_by_full_name_triggered()
{
    onSortCommon(SORT_FULLNAME);
}
void MainWindow::on_actionSort_by_file_size_triggered()
{
	onSortCommon(SORT_SIZE);
}
void MainWindow::on_actionSort_by_wtime_triggered()
{
	onSortCommon(SORT_WTIME);
}
void MainWindow::on_actionSort_by_resolution_triggered()
{
	onSortCommon(SORT_RESOLUTION);
}
void MainWindow::on_actionSort_by_duration_triggered()
{
	onSortCommon(SORT_DURATION);
}
void MainWindow::on_actionSort_by_bitrate_triggered()
{
	onSortCommon(SORT_BITRATE);
}

void MainWindow::on_actionSort_by_open_count_triggered()
{
	onSortCommon(SORT_OPENCOUNT);
}
void MainWindow::on_actionSort_by_last_access_triggered()
{
    onSortCommon(SORT_LASTACCESS);
}

void MainWindow::on_tableView_customContextMenuRequested(const QPoint &pos)
{
    if(ui->tableView->selectionModel()->hasSelection())
    {
        MyContextMenu contextMenu("TableView Context Menu", this);

        contextMenu.addEnablingAction(ui->action_OpenVideo);
        contextMenu.addEnablingAction(ui->action_OpenFolder);

        contextMenu.addSeparator();

        contextMenu.addEnablingAction(ui->action_Copy);

        // sub menu start --->
        QMenu menuCopyOther(tr("C&opy others..."), this);

        QAction actionCopyFilename(tr("&Filename"));
        connect(&actionCopyFilename, SIGNAL(triggered()),
                this, SLOT(OnContextCopySelectedVideoFilename()));
        menuCopyOther.addAction(&actionCopyFilename);

        contextMenu.addMenu(&menuCopyOther);
        // <--- sub menu end

        contextMenu.addSeparator();

        // externl tools ----->
        QMenu menuExternalTools(tr("External &tools..."), this);
        QList< QSharedPointer<QAction> > actExts;
        if(externalTools_.isEmpty())
        {
            QSharedPointer<QAction> act(new QAction(tr("No exteral tools")));
            act->setEnabled(false);
            menuExternalTools.addAction(act.data());
            actExts.append(act);
        }
        else
        {
            for(int i=0 ; i < externalTools_.count(); ++i)
            {
                QSharedPointer<QAction> act(new QAction(externalTools_[i].GetName()));
                connect(act.data(), SIGNAL(triggered()),
                        this, SLOT(OnContextExternalTools()));
                menuExternalTools.addAction(act.data());
                act->setData(i);
                actExts.append(act);
            }
        }
		menuExternalTools.addSeparator();
        menuExternalTools.addAction(ui->action_ExternalTools);

        contextMenu.addMenu(&menuExternalTools);
        // <---- external tools

        contextMenu.addSeparator();

        // Tags tools ----->
        QMenu menuAddTags(tr("Ta&g..."), this);
        QList< QSharedPointer<QAction> > actTags;
        QList<TagItem*> tags;
        pDoc_ && pDoc_->GetAllTags(tags,false);
        QSet<qint64> tagsCurrent;
        pDoc_ && pDoc_->GetTagsFromID(getSelectedID(), tagsCurrent);
        if(tags.isEmpty())
        {
            QSharedPointer<QAction> act(new QAction(tr("No tags")));
            act->setEnabled(false);
            menuAddTags.addAction(act.data());
            actTags.append(act);
        }
        else
        {
            for(TagItem* ti : tags)
            {
                QString text = ti->text();
                QSharedPointer<QAction> act(new QAction(text));
                connect(act.data(), SIGNAL(triggered()),
                        this, SLOT(OnContextAddTags()));
                menuAddTags.addAction(act.data());
                act->setData(ti->tagid());
                act->setCheckable(true);
                if (tagsCurrent.contains(ti->tagid()))
                    act->setChecked(true);
                actTags.append(act);
            }
        }
		menuAddTags.addSeparator();
        menuAddTags.addAction(ui->action_AddNewTag);
        contextMenu.addMenu(&menuAddTags);
        // <---- Tags














        contextMenu.addSeparator();

        QAction actionRename(tr("Re&name"));
        connect(&actionRename, SIGNAL(triggered()),
                this, SLOT(OnContextRename()));
        contextMenu.addAction(&actionRename);
        contextMenu.addSeparator();

        QAction actionRemoveFromDB(tr("&Remove from database"));
        connect(&actionRemoveFromDB, SIGNAL(triggered()),
                this, SLOT(OnContextRemoveFromDatabase()));
        contextMenu.addAction(&actionRemoveFromDB);

        contextMenu.exec(ui->tableView->mapToGlobal(pos));
    }
    else
    {

    }
}


void MainWindow::ScanSelectedDirectory(const bool bAll)
{
    QList<DirectoryItem*> toScan;

    if(bAll || ui->directoryWidget->IsAllItemSelectedOrChecked())
    {
        foreach(DirectoryItem* di, ui->directoryWidget->GetAllNormalItems())
        {
            toScan.append(di);
        }
    }
    else
    {
        if(ui->directoryWidget->selectedItems().isEmpty())
            return;

        for(QListWidgetItem* qi : ui->directoryWidget->selectedItems())
        {
            DirectoryItem* item = (DirectoryItem*)qi;
            if (item->IsMissingItem())
                continue;
            else if(item->IsNormalItem())
                toScan.append(item);
            else if(item->IsAllItem())
            {
                Q_ASSERT_X(false, "selected", "All item must not selected.");
            }
            else
            {
                Q_ASSERT(false);
            }
        }
    }

    foreach(DirectoryItem* di, toScan)
    {
        StartScan(di);
    }
}
void MainWindow::OnDirectoryRemove()
{
    CHECK_DOCUMENT(pDoc_);

    if(ui->directoryWidget->selectedItems().isEmpty())
        return;

	DirectoryItem* item = (DirectoryItem*) ui->directoryWidget->selectedItems()[0];
	if (!item->IsNormalItem())
		return;
    if (!YesNo(this,
               tr("Are you sure you want to remove \"%1\" from list?").arg(item->text())))
		return;
    
    // pDoc_->DeleteDirectory(item);

    BlockedBool bt(&directoryChanging_);
	delete ui->directoryWidget->takeItem(ui->directoryWidget->row(item));

    pDoc_->SetReordered();
}
void MainWindow::OnDirectoryRemoveMissingItems()
{
    if(ui->directoryWidget->selectedItems().isEmpty())
        return;

	DirectoryItem* item = (DirectoryItem*) ui->directoryWidget->selectedItems()[0];

	if (!YesNo(this,
		tr("Are you sure you want to remove missing items from database?")))
	{
		return;
	}
    
	QString dir;
	if(item->IsNormalItem())
		dir = item->text();
	
    gpSQL->RemoveAllMissingEntries(dir);

    itemChangedCommon(true);
}
void MainWindow::OnDirectoryCheckAll()
{
    {
        BlockedBool bt(&directoryChanging_, true, false);

        for (int i = 0; i < ui->directoryWidget->count(); ++i)
        {
            DirectoryItem* item = (DirectoryItem*)ui->directoryWidget->item(i);
            if(item->IsNormalItem())
                item->setCheckState(Qt::Checked);
        }
    }

    itemChangedCommon();
}
void MainWindow::OnDirectoryUncheckAll()
{
    {
        BlockedBool bt(&directoryChanging_, true, false);

        for (int i = 0; i < ui->directoryWidget->count(); ++i)
        {
            DirectoryItem* item = (DirectoryItem*)ui->directoryWidget->item(i);
            if(item->IsNormalItem())
                item->setCheckState(Qt::Unchecked);
        }
    }

    itemChangedCommon();
}



void MainWindow::OnDirectorySortByName()
{
    CHECK_DOCUMENT(pDoc_);

    Q_ASSERT(!directoryChanging_);
    BlockedBool bt(&directoryChanging_, true, false);
	
    ui->directoryWidget->SortNormalItems();

    pDoc_->SetReordered();
}
void MainWindow::directoryWidgetMoveUpCommon(const bool bUp)
{
    CHECK_DOCUMENT(pDoc_);

    if (ui->directoryWidget->selectedItems().isEmpty())
        return;

    Q_ASSERT(!directoryChanging_);
    BlockedBool bt(&directoryChanging_, true, false);

    DirectoryItem* item = (DirectoryItem*)ui->directoryWidget->selectedItems()[0];
    if (!item->IsNormalItem())
        return;

    int row = ui->directoryWidget->row(item);
    if(bUp)
    {
       if (ui->directoryWidget->IsTopNormalItem(row))
            return;
    }
    else
    {
        if (ui->directoryWidget->IsBottomNormalItem(row))
            return;
    }
    item = (DirectoryItem*)ui->directoryWidget->takeItem(row);
    ui->directoryWidget->insertItem(row + (bUp ? -1:1), item);
    item->setSelected(true);
    ui->directoryWidget->setFocus();

    pDoc_->SetReordered();
}
void MainWindow::OnDirectoryMoveUp()
{
    directoryWidgetMoveUpCommon(true);
}
void MainWindow::OnDirectoryMoveDown()
{
    directoryWidgetMoveUpCommon(false);
}

void MainWindow::on_directoryWidget_customContextMenuRequested(const QPoint &pos)
{
    DirectoryItem* di = (DirectoryItem*)ui->directoryWidget->itemAt(pos);

    if(!di)
    {
        MyContextMenu menu("DirectoryWidget Context Menu",this);
        menu.addEnablingAction(ui->action_AddFolder);
        menu.addEnablingAction(ui->action_Paste);
        menu.addSeparator();

        QAction actCheckAll(tr("&Check All"));
        connect(&actCheckAll, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryCheckAll()));
        menu.addAction(&actCheckAll);

        QAction actUncheckAll(tr("&Uncheck All"));
        connect(&actUncheckAll, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryUncheckAll()));
        menu.addAction(&actUncheckAll);

        menu.addSeparator();

        QAction actSortByName(tr("&Sort by name"));
        connect(&actSortByName, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectorySortByName()));
        menu.addAction(&actSortByName);

        menu.exec(ui->directoryWidget->mapToGlobal(pos));
    }
    else
    {
        MyContextMenu menu("DirectoryWidget Context Menu",this);
        menu.addEnablingAction(ui->action_Copy, di->IsNormalItem());
        menu.addEnablingAction(ui->action_OpenFolder, di->IsNormalItem());
        menu.addSeparator();


        menu.addEnablingAction(ui->action_ScanSelectedDirectory);
        menu.addSeparator();

        QAction actCheckAll(tr("&Check All"));
        connect(&actCheckAll, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryCheckAll()));
        menu.addAction(&actCheckAll);

        QAction actUncheckAll(tr("&Uncheck All"));
        connect(&actUncheckAll, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryUncheckAll()));
        menu.addAction(&actUncheckAll);

        menu.addSeparator();

        QAction actMoveUp(tr("Move &up"));
        connect(&actMoveUp, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryMoveUp()));
        menu.addAction(&actMoveUp);

        QAction actMoveDown(tr("Move &down"));
        connect(&actMoveDown, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryMoveDown()));
        menu.addAction(&actMoveDown);

        menu.addSeparator();

        QAction actSortByName(tr("&Sort by name"));
        connect(&actSortByName, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectorySortByName()));
        menu.addAction(&actSortByName);


        menu.addSeparator();

        QAction actRemove(tr("Re&move this directory from list"));
        actRemove.setEnabled(di->IsNormalItem());
        connect(&actRemove, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryRemove()));
        menu.addAction(&actRemove);

        QAction actRemoveMissingItems(tr("R&emove missing items from database"));
        connect(&actRemoveMissingItems, SIGNAL(triggered(bool)),
            this, SLOT(OnDirectoryRemoveMissingItems()));
        menu.addAction(&actRemoveMissingItems);

        menu.exec(ui->directoryWidget->mapToGlobal(pos));
    }
}

void MainWindow::on_ShowMissingClicked_common(bool bNextCheck)
{
    if (!IsInitialized() || IsClosed())
        return;

    static bool sIn = false;
    if(sIn)
        return;
    BlockedBool bb(&sIn);

    if(!bNextCheck)
    {
        if(!YesNo(
            this,
            tr("Excluding an item which has missig files will take some time for querying. Do you wan to continue?")))
        {
            ui->action_ShowMissingFiles->setChecked(!bNextCheck);
            tbShowNonExistant_->setChecked(!bNextCheck);

            return;
        }
    }
    ui->action_ShowMissingFiles->setChecked(bNextCheck);
    tbShowNonExistant_->setChecked(bNextCheck);
    GetSqlAllSetTable(lastQueriedDirs_, lastQueriedIsTagValid_ ? &lastQueriedTaggedIDs_:nullptr);
}
void MainWindow::OnShowMissingClicked()
{
    on_ShowMissingClicked_common(tbShowNonExistant_->isChecked());
}

void MainWindow::OnLimitFirstTriggered(bool checked)
{
    Q_UNUSED(checked);

    Q_ASSERT(limitManager_);
    limitManager_->SetIndexFirst();
    itemChangedCommon(true);
}
void MainWindow::OnLimitPrevTriggered(bool checked)
{
    Q_UNUSED(checked);

    Q_ASSERT(limitManager_);
    if(limitManager_->Decrement())
    {
        itemChangedCommon(true);
    }
}

void MainWindow::OnLimitNextTriggered(bool checked)
{
    Q_UNUSED(checked);

    Q_ASSERT(limitManager_);
    if(limitManager_->Increment())
    {
        itemChangedCommon(true);
    }
}
void MainWindow::OnLimitLastTriggered(bool checked)
{
    Q_UNUSED(checked);

    Q_ASSERT(limitManager_);
    limitManager_->SetIndexLast();
    itemChangedCommon(true);
}
void MainWindow::OnCmbLimitCurrentIndexChanged(int index)
{
    // qDebug() << index;
    Q_UNUSED(index);
    if(limitManager_->IsBlocking())
        return;
    itemChangedCommon(true);
}

bool MainWindow::LimitManager::Decrement()
{
    int ci = cmb_->currentIndex();
    --ci;
    if(ci < 0)
        return false;
    BlockedBool bb(&blocking_);
    cmb_->setCurrentIndex(ci);
    return true;
}
bool MainWindow::LimitManager::Increment()
{
    int ci = cmb_->currentIndex();
    ++ci;
    if(cmb_->count() <= ci)
        return false;
    BlockedBool bb(&blocking_);
    cmb_->setCurrentIndex(ci);
    return true;
}


void MainWindow::langChanged_common(const QString& lang)
{
    if(settings_.value(KEY_LANGUAGE)==lang)
        return;
    settings_.setValue(KEY_LANGUAGE, lang);
    if(YesNo(this,
          tr("Application needs to restart to effect the change. Do you want to restart application now?")))
    {
        gReboot = true;
        this->close();
        return;
    }
}

void MainWindow::OnMenuStyle()
{
    QAction* pA = (QAction*)QObject::sender();
    QString style = pA->text();

    settings_.setValue(KEY_STYLE, style);
    askRebootClose();
}
void MainWindow::onMenuStyle_AboutToShow()
{
    QList<QAction*> actions = ui->menu_Style->actions();
    for(int i=0 ; i < actions.count(); ++i)
    {
        QAction* pA = actions[i];
        ui->menu_Folder->removeAction(pA);
        delete pA;
    }
    QString currentStyle = QApplication::style()->objectName();
    for(const QString& style : QStyleFactory::keys())
    {
        QAction* pA = new QAction(style);
        if(style.compare(currentStyle, Qt::CaseInsensitive)==0)
        {
            pA->setCheckable(true);
            pA->setChecked(true);
        }
        connect(pA, &QAction::triggered,
                this, &MainWindow::OnMenuStyle);
        ui->menu_Style->addAction(pA);
    }
}
void MainWindow::onMenuLanguage_AboutToShow()
{
    foreach (QAction *action, ui->menu_Language->actions())
    {
        if (action->isSeparator())
            continue;
        else if (action->menu())
            continue;
        else
        {
            action->setChecked(false);
        }
    }

    QString lang = settings_.valueString(KEY_LANGUAGE);
    if(lang.isEmpty())
    {
        ui->action_SystemDefault->setChecked(true);
    }
    else if(lang=="English")
    {
        ui->action_English->setChecked(true);
    }
    else if(lang=="Japanese")
    {
        ui->action_Japanese->setChecked(true);
    }
    else
    {
        Q_ASSERT(false);
    }
}
void MainWindow::on_action_SystemDefault_triggered()
{
    langChanged_common(QString());
}
void MainWindow::on_action_English_triggered()
{
    langChanged_common("English");
}
void MainWindow::on_action_Japanese_triggered()
{
    langChanged_common("Japanese");
}
