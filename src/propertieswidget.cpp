/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#include <QTimer>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QSplitter>
#include <QHeaderView>
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QDesktopServices>
#include <QInputDialog>
#include <libtorrent/version.hpp>
#include "propertieswidget.h"
#include "transferlistwidget.h"
#include "torrentpersistentdata.h"
#include "bittorrent.h"
#include "proplistdelegate.h"
#include "torrentfilesmodel.h"
#include "peerlistwidget.h"
#include "trackerlist.h"
#include "GUI.h"
#include "downloadedpiecesbar.h"
#include "pieceavailabilitybar.h"

#ifdef Q_WS_MAC
#define DEFAULT_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px; margin-left: 8px; margin-right: 8px;}"
#define SELECTED_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px;background-color: rgb(255, 208, 105); margin-left: 8px; margin-right: 8px;}"
#else
#define DEFAULT_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px; margin-left: 3px; margin-right: 3px;}"
#define SELECTED_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px;background-color: rgb(255, 208, 105); margin-left: 3px; margin-right: 3px;}"
#endif

PropertiesWidget::PropertiesWidget(QWidget *parent, GUI* main_window, TransferListWidget *transferList, Bittorrent* BTSession):
    QWidget(parent), transferList(transferList), main_window(main_window), BTSession(BTSession) {
  setupUi(this);
  state = VISIBLE;
  setEnabled(false);
  // Buttons stylesheet
  trackers_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  peers_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  url_seeds_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  files_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  main_infos_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  main_infos_button->setShortcut(QKeySequence(QString::fromUtf8("Alt+P")));

  // Set Properties list model
  PropListModel = new TorrentFilesModel();
  filesList->setModel(PropListModel);
  PropDelegate = new PropListDelegate(this);
  filesList->setItemDelegate(PropDelegate);

  // SIGNAL/SLOTS
  connect(filesList, SIGNAL(clicked(const QModelIndex&)), filesList, SLOT(edit(const QModelIndex&)));
  connect(collapseAllButton, SIGNAL(clicked()), filesList, SLOT(collapseAll()));
  connect(expandAllButton, SIGNAL(clicked()), filesList, SLOT(expandAll()));
  connect(filesList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
  connect(filesList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openDoubleClickedFile(QModelIndex)));
  connect(PropListModel, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
  connect(addWS_button, SIGNAL(clicked()), this, SLOT(askWebSeed()));
  connect(deleteWS_button, SIGNAL(clicked()), this, SLOT(deleteSelectedUrlSeeds()));
  connect(transferList, SIGNAL(currentTorrentChanged(QTorrentHandle&)), this, SLOT(loadTorrentInfos(QTorrentHandle &)));
  connect(PropDelegate, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
  connect(stackedProperties, SIGNAL(currentChanged(int)), this, SLOT(loadDynamicData()));
  connect(BTSession, SIGNAL(savePathChanged(QTorrentHandle&)), this, SLOT(updateSavePath(QTorrentHandle&)));

  // Downloaded pieces progress bar
  downloaded_pieces = new DownloadedPiecesBar(this);
  ProgressHLayout->insertWidget(1, downloaded_pieces);
  // Pieces availability bar
  pieces_availability = new PieceAvailabilityBar(this);
  ProgressHLayout_2->insertWidget(1, pieces_availability);
  // Tracker list
  trackerList = new TrackerList(this);
#if LIBTORRENT_VERSION_MINOR > 14
  trackerUpButton->setVisible(false);
  trackerDownButton->setVisible(false);
#else
  connect(trackerUpButton, SIGNAL(clicked()), trackerList, SLOT(moveSelectionUp()));
  connect(trackerDownButton, SIGNAL(clicked()), trackerList, SLOT(moveSelectionDown()));
#endif
  horizontalLayout_trackers->insertWidget(0, trackerList);
  // Peers list
  peersList = new PeerListWidget(this);
  peerpage_layout->addWidget(peersList);
  // Dynamic data refresher
  refreshTimer = new QTimer(this);
  connect(refreshTimer, SIGNAL(timeout()), this, SLOT(loadDynamicData()));
  refreshTimer->start(3000); // 3sec
}

PropertiesWidget::~PropertiesWidget() {
  delete refreshTimer;
  delete trackerList;
  delete peersList;
  delete downloaded_pieces;
  delete pieces_availability;
  delete PropListModel;
  delete PropDelegate;
}

void PropertiesWidget::showPiecesAvailability(bool show) {
  avail_pieces_lbl->setVisible(show);
  pieces_availability->setVisible(show);
  avail_average_lbl->setVisible(show);
  if(show || (!show && !downloaded_pieces->isVisible()))
    line_2->setVisible(show);
}

void PropertiesWidget::showPiecesDownloaded(bool show) {
  downloaded_pieces_lbl->setVisible(show);
  downloaded_pieces->setVisible(show);
  progress_lbl->setVisible(show);
  if(show || (!show && !pieces_availability->isVisible()))
    line_2->setVisible(show);
}

void PropertiesWidget::reduce() {
  if(state == VISIBLE) {
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    slideSizes = hSplitter->sizes();
    stackedProperties->setVisible(false);
    QList<int> sizes;
    sizes << hSplitter->geometry().height()-30 << 30;
    hSplitter->setSizes(sizes);
    hSplitter->handle(1)->setVisible(false);
    hSplitter->handle(1)->setDisabled(true);
    state = REDUCED;
  }
}

void PropertiesWidget::slide() {
  if(state == REDUCED) {
    stackedProperties->setVisible(true);
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    hSplitter->handle(1)->setDisabled(false);
    hSplitter->handle(1)->setVisible(true);
    hSplitter->setSizes(slideSizes);
    state = VISIBLE;
    // Force refresh
    loadDynamicData();
  }
}

void PropertiesWidget::clear() {
  qDebug("Clearing torrent properties");
  save_path->clear();
  lbl_creationDate->clear();
  hash_lbl->clear();
  comment_text->clear();
  progress_lbl->clear();
  trackerList->clear();
  downloaded_pieces->clear();
  pieces_availability->clear();
  avail_average_lbl->clear();
  wasted->clear();
  upTotal->clear();
  dlTotal->clear();
  peersList->clear();
  lbl_uplimit->clear();
  lbl_dllimit->clear();
  lbl_elapsed->clear();
  lbl_connections->clear();
  shareRatio->clear();
  listWebSeeds->clear();
  PropListModel->clear();
  showPiecesAvailability(false);
  showPiecesDownloaded(false);
  setEnabled(false);
}

const QTorrentHandle& PropertiesWidget::getCurrentTorrent() const {
  return h;
}

Bittorrent* PropertiesWidget::getBTSession() const {
  return BTSession;
}

void PropertiesWidget::updateSavePath(QTorrentHandle& _h) {
  if(h.is_valid() && h == _h) {
    QString p = TorrentPersistentData::getSavePath(h.hash());
    if(p.isEmpty())
      p = h.save_path();
#ifdef Q_WS_WIN
    p = p.replace("/", "\\");
#endif
    save_path->setText(p);
  }
}

void PropertiesWidget::loadTorrentInfos(QTorrentHandle &_h) {
  clear();
  h = _h;
  if(!h.is_valid()) {
    clear();
    return;
  }
  setEnabled(true);

  try {
    // Save path
    QString p = TorrentPersistentData::getSavePath(h.hash());
    if(p.isEmpty())
      p = h.save_path();
#ifdef Q_WS_WIN
    p = p.replace("/", "\\");
#endif
    save_path->setText(p);
    // Creation date
    lbl_creationDate->setText(h.creation_date());
    // Hash
    hash_lbl->setText(h.hash());
    // Comment
    comment_text->setHtml(h.comment());
    // URL seeds
    loadUrlSeeds();
    // List files in torrent
    PropListModel->clear();
    PropListModel->setupModelData(h.get_torrent_info());
    // Expand first item if possible
    filesList->expand(PropListModel->index(0, 0));
  } catch(invalid_handle e) {

  }
  // Load dynamic data
  loadDynamicData();
}

void PropertiesWidget::readSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QList<int> contentColsWidths = misc::intListfromStringList(settings.value(QString::fromUtf8("TorrentProperties/filesColsWidth")).toStringList());
  if(contentColsWidths.empty()) {
    filesList->header()->resizeSection(0, 300);
  } else {
    for(int i=0; i<contentColsWidths.size(); ++i) {
      filesList->setColumnWidth(i, contentColsWidths.at(i));
    }
  }
  // Restore splitter sizes
  QStringList sizes_str = settings.value(QString::fromUtf8("TorrentProperties/SplitterSizes"), QString()).toString().split(",");
  if(sizes_str.size() == 2) {
    slideSizes << sizes_str.first().toInt();
    slideSizes << sizes_str.last().toInt();
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    hSplitter->setSizes(slideSizes);
  }
  if(!settings.value("TorrentProperties/Visible", false).toBool()) {
    reduce();
  } else {
    main_infos_button->setStyleSheet(SELECTED_BUTTON_CSS);
    //setEnabled(false);
  }
}

void PropertiesWidget::saveSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.setValue("TorrentProperties/Visible", state==VISIBLE);
  QStringList contentColsWidths;
  for(int i=0; i<PropListModel->columnCount(); ++i) {
    contentColsWidths << QString::number(filesList->columnWidth(i));
  }
  settings.setValue(QString::fromUtf8("TorrentProperties/filesColsWidth"), contentColsWidths);
  // Splitter sizes
  QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
  QList<int> sizes;
  if(state == VISIBLE)
    sizes = hSplitter->sizes();
  else
    sizes = slideSizes;
  qDebug("Sizes: %d", sizes.size());
  if(sizes.size() == 2) {
    settings.setValue(QString::fromUtf8("TorrentProperties/SplitterSizes"), QVariant(QString::number(sizes.first())+','+QString::number(sizes.last())));
  }
}

void PropertiesWidget::reloadPreferences() {
  // Take program preferences into consideration
  peersList->updatePeerHostNameResolutionState();
  peersList->updatePeerCountryResolutionState();
}

void PropertiesWidget::loadDynamicData() {
  // Refresh only if the torrent handle is valid and if visible
  if(!h.is_valid() || main_window->getCurrentTabIndex() != TAB_TRANSFER || state != VISIBLE) return;
  try {
    // Transfer infos
    if(stackedProperties->currentIndex() == MAIN_TAB) {
      wasted->setText(misc::friendlyUnit(h.total_failed_bytes()+h.total_redundant_bytes()));
      upTotal->setText(misc::friendlyUnit(h.all_time_upload()) + " ("+misc::friendlyUnit(h.total_payload_upload())+" "+tr("this session")+")");
      dlTotal->setText(misc::friendlyUnit(h.all_time_download()) + " ("+misc::friendlyUnit(h.total_payload_download())+" "+tr("this session")+")");
      if(h.upload_limit() <= 0)
        lbl_uplimit->setText(QString::fromUtf8("∞"));
      else
        lbl_uplimit->setText(misc::friendlyUnit(h.upload_limit())+tr("/s", "/second (i.e. per second)"));
      if(h.download_limit() <= 0)
        lbl_dllimit->setText(QString::fromUtf8("∞"));
      else
        lbl_dllimit->setText(misc::friendlyUnit(h.download_limit())+tr("/s", "/second (i.e. per second)"));
      QString elapsed_txt = misc::userFriendlyDuration(h.active_time());
      if(h.is_seed()) {
        elapsed_txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(h.seeding_time()))+")";
      }
      lbl_elapsed->setText(elapsed_txt);
      if(h.connections_limit() > 0)
        lbl_connections->setText(QString::number(h.num_connections())+" ("+tr("%1 max", "e.g. 10 max").arg(QString::number(h.connections_limit()))+")");
      else
        lbl_connections->setText(QString::number(h.num_connections()));
      // Update ratio info
      const double ratio = BTSession->getRealRatio(h.hash());
      if(ratio > 100.)
        shareRatio->setText(QString::fromUtf8("∞"));
      else
        shareRatio->setText(QString(QByteArray::number(ratio, 'f', 2)));
      if(!h.is_seed()) {
        showPiecesDownloaded(true);
        // Downloaded pieces
        bitfield bf(h.get_torrent_info().num_pieces(), 0);
        h.downloading_pieces(bf);
        downloaded_pieces->setProgress(h.pieces(), bf);
        // Pieces availability
        if(h.has_metadata() && !h.is_paused() && !h.is_queued() && !h.is_checking()) {
          showPiecesAvailability(true);
          std::vector<int> avail;
          h.piece_availability(avail);
          pieces_availability->setAvailability(avail);
          avail_average_lbl->setText(QString::number(h.get_torrent_handle().status().distributed_copies, 'f', 1));
        } else {
          showPiecesAvailability(false);
        }
        // Progress
        progress_lbl->setText(QString::number(h.progress()*100., 'f', 1)+"%");
      } else {
        showPiecesAvailability(false);
        showPiecesDownloaded(false);
      }
      return;
    }
    if(stackedProperties->currentIndex() == TRACKERS_TAB) {
      // Trackers
      trackerList->loadTrackers();
      return;
    }
    if(stackedProperties->currentIndex() == PEERS_TAB) {
      // Load peers
      peersList->loadPeers(h);
      return;
    }
    if(stackedProperties->currentIndex() == FILES_TAB) {
      // Files progress
      if(h.is_valid() && h.has_metadata()) {
        if(PropListModel->rowCount() == 0) {
          PropListModel->setupModelData(h.get_torrent_info());
          // Expand first item if possible
          filesList->expand(PropListModel->index(0, 0));
        }
        std::vector<size_type> fp;
        h.file_progress(fp);
        PropListModel->updateFilesPriorities(h.file_priorities());
        PropListModel->updateFilesProgress(fp);
      }
    }
  } catch(invalid_handle e) {}
}

void PropertiesWidget::loadUrlSeeds(){
  listWebSeeds->clear();
  qDebug("Loading URL seeds");
  const QStringList &hc_seeds = h.url_seeds();
  // Add url seeds
  foreach(const QString &hc_seed, hc_seeds){
    qDebug("Loading URL seed: %s", qPrintable(hc_seed));
    new QListWidgetItem(hc_seed, listWebSeeds);
  }
}

/* Tab buttons */
QPushButton* PropertiesWidget::getButtonFromIndex(int index) {
  switch(index) {
  case TRACKERS_TAB:
    return trackers_button;
  case PEERS_TAB:
    return peers_button;
  case URLSEEDS_TAB:
    return url_seeds_button;
  case FILES_TAB:
    return files_button;
  default:
    return main_infos_button;
  }
}

void PropertiesWidget::on_main_infos_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == MAIN_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(MAIN_TAB);
    main_infos_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_trackers_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == TRACKERS_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(TRACKERS_TAB);
    trackers_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_peers_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == PEERS_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(PEERS_TAB);
    peers_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_url_seeds_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == URLSEEDS_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(URLSEEDS_TAB);
    url_seeds_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_files_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == FILES_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(FILES_TAB);
    files_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::openDoubleClickedFile(QModelIndex index) {
  if(!index.isValid()) return;
  if(!h.is_valid() || !h.has_metadata()) return;
  if(PropListModel->getType(index) == TFILE) {
    int i = PropListModel->getFileIndex(index);
    const QDir &saveDir(h.save_path());
    const QString &filename = misc::toQStringU(h.get_torrent_info().file_at(i).path.string());
    const QString &file_path = QDir::cleanPath(saveDir.absoluteFilePath(filename));
    qDebug("Trying to open file at %s", qPrintable(file_path));
#if LIBTORRENT_VERSION_MINOR > 14
    // Flush data
    h.flush_cache();
#endif
    if(QFile::exists(file_path)) {
#ifdef Q_WS_WIN
      QDesktopServices::openUrl(QUrl("file:///"+file_path));
#else
      QDesktopServices::openUrl(QUrl("file://"+file_path));
#endif
    } else {
      QMessageBox::warning(this, tr("I/O Error"), tr("This file does not exist yet."));
    }
  } else {
    // FOLDER
    QStringList path_items;
    path_items << index.data().toString();
    QModelIndex parent = PropListModel->parent(index);
    while(parent.isValid()) {
      path_items.prepend(parent.data().toString());
      parent = PropListModel->parent(parent);
    }
    const QDir &saveDir(h.save_path());
    const QString &filename = path_items.join(QDir::separator());
    const QString &file_path = QDir::cleanPath(saveDir.absoluteFilePath(filename));
    qDebug("Trying to open folder at %s", qPrintable(file_path));
#if LIBTORRENT_VERSION_MINOR > 14
    // Flush data
    h.flush_cache();
#endif
    if(QFile::exists(file_path)) {
#ifdef Q_WS_WIN
      QDesktopServices::openUrl(QUrl("file:///"+file_path));
#else
      QDesktopServices::openUrl(QUrl("file://"+file_path));
#endif
    } else {
      QMessageBox::warning(this, tr("I/O Error"), tr("This folder does not exist yet."));
    }
  }
}

void PropertiesWidget::displayFilesListMenu(const QPoint&){
  QMenu myFilesLlistMenu;
  QModelIndexList selectedRows = filesList->selectionModel()->selectedRows(0);
  QAction *actRename = 0;
  if(selectedRows.size() == 1) {
    actRename = myFilesLlistMenu.addAction(QIcon(QString::fromUtf8(":/Icons/oxygen/edit_clear.png")), tr("Rename..."));
    myFilesLlistMenu.addSeparator();
  }
  QMenu subMenu;
  subMenu.setTitle(tr("Priority"));
  subMenu.addAction(actionNormal);
  subMenu.addAction(actionHigh);
  subMenu.addAction(actionMaximum);
  myFilesLlistMenu.addMenu(&subMenu);
  // Call menu
  const QAction *act = myFilesLlistMenu.exec(QCursor::pos());
  if(act) {
    if(act == actRename) {
      renameSelectedFile();
    } else {
      int prio = 1;
      if(act == actionHigh) {
        prio = HIGH;
      } else {
        if(act == actionMaximum) {
          prio = MAXIMUM;
        }
      }
      qDebug("Setting files priority");
      foreach(QModelIndex index, selectedRows) {
        qDebug("Setting priority(%d) for file at row %d", prio, index.row());
        PropListModel->setData(PropListModel->index(index.row(), PRIORITY, index.parent()), prio);
      }
      // Save changes
      filteredFilesChanged();
    }
  }
}

void PropertiesWidget::renameSelectedFile() {
  const QModelIndexList &selectedIndexes = filesList->selectionModel()->selectedRows(0);
  Q_ASSERT(selectedIndexes.size() == 1);
  const QModelIndex &index = selectedIndexes.first();
  // Ask for new name
  bool ok;
  QString new_name_last = QInputDialog::getText(this, tr("Rename the file"),
                                                tr("New name:"), QLineEdit::Normal,
                                                index.data().toString(), &ok);
  if (ok && !new_name_last.isEmpty()) {
    if(!misc::isValidFileSystemName(new_name_last)) {
      QMessageBox::warning(this, tr("The file could not be renamed"),
                           tr("This file name contains forbidden characters, please choose a different one."),
                           QMessageBox::Ok);
      return;
    }
    if(PropListModel->getType(index)==TFILE) {
      // File renaming
      const int file_index = PropListModel->getFileIndex(index);
      if(!h.is_valid() || !h.has_metadata()) return;
      QString old_name = misc::toQStringU(h.get_torrent_info().file_at(file_index).path.string());
      old_name = old_name.replace("\\", "/");
      if(old_name.endsWith(".!qB") && !new_name_last.endsWith(".!qB")) {
        new_name_last += ".!qB";
      }
      QStringList path_items = old_name.split("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_name = path_items.join("/");
      if(old_name == new_name) {
        qDebug("Name did not change");
        return;
      }
      new_name = QDir::cleanPath(new_name);
      // Check if that name is already used
      for(int i=0; i<h.num_files(); ++i) {
        if(i == file_index) continue;
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
        if(misc::toQStringU(h.get_torrent_info().file_at(i).path.string()).compare(new_name, Qt::CaseSensitive) == 0) {
#else
          if(misc::toQStringU(h.get_torrent_info().file_at(i).path.string()).compare(new_name, Qt::CaseInsensitive) == 0) {
#endif
            // Display error message
            QMessageBox::warning(this, tr("The file could not be renamed"),
                                 tr("This name is already in use in this folder. Please use a different name."),
                                 QMessageBox::Ok);
            return;
          }
        }
        const bool force_recheck = QFile::exists(h.save_path()+QDir::separator()+new_name);
        qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(new_name));
        h.rename_file(file_index, new_name);
        // Force recheck
        if(force_recheck) h.force_recheck();
        // Rename if torrent files model too
        if(new_name_last.endsWith(".!qB"))
          new_name_last.chop(4);
        PropListModel->setData(index, new_name_last);
      } else {
        // Folder renaming
        QStringList path_items;
        path_items << index.data().toString();
        QModelIndex parent = PropListModel->parent(index);
        while(parent.isValid()) {
          path_items.prepend(parent.data().toString());
          parent = PropListModel->parent(parent);
        }
        const QString &old_path = path_items.join("/");
        path_items.removeLast();
        path_items << new_name_last;
        QString new_path = path_items.join("/");
        if(!new_path.endsWith("/")) new_path += "/";
        // Check for overwriting
        const int num_files = h.num_files();
        for(int i=0; i<num_files; ++i) {
          const QString current_name = misc::toQStringU(h.get_torrent_info().file_at(i).path.string());
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
          if(current_name.startsWith(new_path, Qt::CaseSensitive)) {
#else
            if(current_name.startsWith(new_path, Qt::CaseInsensitive)) {
#endif
              QMessageBox::warning(this, tr("The folder could not be renamed"),
                                   tr("This name is already in use in this folder. Please use a different name."),
                                   QMessageBox::Ok);
              return;
            }
          }
          bool force_recheck = false;
          // Replace path in all files
          for(int i=0; i<num_files; ++i) {
            const QString &current_name = misc::toQStringU(h.get_torrent_info().file_at(i).path.string());
            if(current_name.startsWith(old_path)) {
              QString new_name = current_name;
              new_name.replace(0, old_path.length(), new_path);
              if(!force_recheck && QDir(h.save_path()).exists(new_name))
                force_recheck = true;
              new_name = QDir::cleanPath(new_name);
              qDebug("Rename %s to %s", qPrintable(current_name), qPrintable(new_name));
              h.rename_file(i, new_name);
            }
          }
          // Force recheck
          if(force_recheck) h.force_recheck();
          // Rename folder in torrent files model too
          PropListModel->setData(index, new_name_last);
          // Remove old folder
          const QDir old_folder(h.save_path()+"/"+old_path);
          int timeout = 10;
          while(!misc::removeEmptyTree(old_folder.absolutePath()) && timeout > 0) {
            SleeperThread::msleep(100);
            --timeout;
          }
        }
      }
    }

    void PropertiesWidget::askWebSeed(){
      bool ok;
      // Ask user for a new url seed
      const QString &url_seed = QInputDialog::getText(this, tr("New url seed", "New HTTP source"),
                                               tr("New url seed:"), QLineEdit::Normal,
                                               QString::fromUtf8("http://www."), &ok);
      if(!ok) return;
      qDebug("Adding %s web seed", qPrintable(url_seed));
      if(!listWebSeeds->findItems(url_seed, Qt::MatchFixedString).empty()) {
        QMessageBox::warning(this, tr("qBittorrent"),
                             tr("This url seed is already in the list."),
                             QMessageBox::Ok);
        return;
      }
      h.add_url_seed(url_seed);
      // Refresh the seeds list
      loadUrlSeeds();
    }

    void PropertiesWidget::deleteSelectedUrlSeeds(){
      const QList<QListWidgetItem *> &selectedItems = listWebSeeds->selectedItems();
      bool change = false;
      foreach(const QListWidgetItem *item, selectedItems){
        QString url_seed = item->text();
        h.remove_url_seed(url_seed);
        change = true;
      }
      if(change){
        // Refresh list
        loadUrlSeeds();
      }
    }

    bool PropertiesWidget::applyPriorities() {
      qDebug("Saving pieces priorities");
      const std::vector<int> &priorities = PropListModel->getFilesPriorities(h.get_torrent_info().num_files());
      bool first_last_piece_first = false;
      // Save first/last piece first option state
      if(h.first_last_piece_first())
        first_last_piece_first = true;
      // Prioritize the files
      qDebug("prioritize files: %d", priorities[0]);
      h.prioritize_files(priorities);
      // Restore first/last piece first option if necessary
      if(first_last_piece_first)
        h.prioritize_first_last_piece(true);
      return true;
    }


    void PropertiesWidget::on_changeSavePathButton_clicked() {
      if(!h.is_valid()) return;
      QString dir;
      const QDir saveDir(h.save_path());
      if(saveDir.exists()){
        dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), h.save_path());
      }else{
        dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
      }
      if(!dir.isNull()){
        // Check if savePath exists
        QDir savePath(misc::expandPath(dir));
        if(!savePath.exists()){
          if(!savePath.mkpath(savePath.absolutePath())){
            QMessageBox::critical(0, tr("Save path creation error"), tr("Could not create the save path"));
            return;
          }
        }
        // Actually move storage
        if(!BTSession->useTemporaryFolder() || h.is_seed())
          h.move_storage(savePath.absolutePath());
        // Update save_path in dialog
        QString display_path = savePath.absolutePath();
#ifdef Q_WS_WIN
        display_path = display_path.replace("/", "\\");
#endif
        save_path->setText(display_path);
      }
    }

    void PropertiesWidget::filteredFilesChanged() {
      if(h.is_valid()) {
        applyPriorities();
      }
    }
