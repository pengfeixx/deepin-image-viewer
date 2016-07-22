#ifndef IMAGESVIEW_H
#define IMAGESVIEW_H

#include "topalbumtips.h"
#include "controller/databasemanager.h"
#include <QWidget>
#include <QScrollArea>
#include <QJsonObject>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QObject>

class PopupMenuManager;
class QStandardItemModel;
class QStackedWidget;
class SignalManager;
class ThumbnailListView;
class ImagesView : public QScrollArea
{
    Q_OBJECT
public:
    explicit ImagesView(QWidget *parent = 0);

    void insertItem(const DatabaseManager::ImageInfo &info);
    void setAlbum(const QString &album);
    void setIconSize(const QSize &iconSize);

    int count() const;
    bool removeItem(const QString &name);

    QString getCurrentAlbum() const;
    QSize iconSize() const;
    QStringList selectedImagesNameList() const;
    QStringList selectedImagesPathList() const;

signals:
    void startSlideShow(const QStringList &paths, const QString &path);
    void viewImage(const QString &path,
                   const QStringList &paths,
                   bool fullscreen = false);

protected:
    void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *e) Q_DECL_OVERRIDE;

private:
    enum MenuItemId {
        IdView,
        IdFullScreen,
        IdStartSlideShow,
        IdAddToAlbum,
        IdExport,
        IdCopy,
        IdMoveToTrash,
        IdRemoveFromAlbum,
        IdEdit,
        IdAddToFavorites,
        IdRemoveFromFavorites,
        IdRotateClockwise,
        IdRotateCounterclockwise,
        IdLabel,
        IdSetAsWallpaper,
        IdDisplayInFileManager,
        IdImageInfo,
        IdSubMenu,
        IdSeparator
    };

    void initListView();
    void initStack();
    void initTopTips();
    void updateMenuContents();
    void updateStack();
    void updateThumbnail(const QString &path);
    void updateTopTipsRect();
    void onMenuItemClicked(int menuId, const QString &text);

    bool allInAlbum(const QStringList &names, const QString &album);

    QJsonObject createAlbumMenuObj();
    QString createMenuContent();
    QJsonValue createMenuItem(const MenuItemId id,
                              const QString &text,
                              const bool isSeparator = false,
                              const QString &shortcut = "",
                              const QJsonObject &subMenu = QJsonObject());
    const QStringList paths();

private:
    QString m_album;
    QStackedWidget *m_stackWidget;
    TopAlbumTips *m_topTips;
    ThumbnailListView *m_view;
    QVBoxLayout *m_contentLayout;
    PopupMenuManager *m_popupMenu;
    DatabaseManager *m_dbManager;
    SignalManager *m_sManager;
};

#endif // IMAGESVIEW_H
