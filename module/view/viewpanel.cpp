#include "viewpanel.h"
#include "imageinfowidget.h"
#include "utils/imgutil.h"
#include "controller/signalmanager.h"
#include "slideeffect/slideeffectplayer.h"
#include "controller/popupmenumanager.h"
#include <darrowrectangle.h>
#include <dimagebutton.h>
#include <QBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QMenu>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDebug>

using namespace Dtk::Widget;

namespace {

const QString SHORTCUT_SPLIT_FLAG = "@-_-@";

}  // namespace

ViewPanel::ViewPanel(QWidget *parent)
    : ModulePanel(parent),
      m_popupMenu(new PopupMenuManager(this)),
      m_signalManager(SignalManager::instance()),
      m_dbManager(DatabaseManager::instance())
{
    m_slide = new SlideEffectPlayer(this);
    m_view = new ImageWidget();
    QHBoxLayout *hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->addWidget(m_view);

    m_nav = new NavigationWidget(this);
    setContextMenuPolicy(Qt::CustomContextMenu);
    initConnect();
}

void ViewPanel::initConnect() {
    connect(m_slide, &SlideEffectPlayer::stepChanged, [this](int steps){
        m_current += steps;
    });
    connect(m_slide, &SlideEffectPlayer::currentImageChanged,
            [this](const QString& path){
        // Slide image size is widget size
        m_nav->setImage(QImage(path).scaled(m_slide->frameSize(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        if (m_info) {
            m_info->setImagePath(path);
        }
    });
    connect(m_slide, &SlideEffectPlayer::frameReady,
            [this](const QImage& image) {
        m_view->setImage(image);
    });
    connect(m_signalManager, &SignalManager::gotoPanel, [this](){
        m_slide->stop();
    });
    connect(m_signalManager, &SignalManager::viewImage, [this](QString path) {
        DatabaseManager::ImageInfo info =
                DatabaseManager::instance()->getImageInfoByPath(path);
        m_infos = DatabaseManager::instance()->getImageInfosByTime(info.time);
        m_current = std::find_if(m_infos.cbegin(), m_infos.cend(),
                                 [&](const DatabaseManager::ImageInfo info){
            return info.path == path;}
        );
        openImage(path);
    });
    connect(m_view, &ImageWidget::rotated, [this](int degree) {
        const QTransform t = QTransform().rotate(degree);
        QImage img = m_view->image().transformed(t);
        utils::saveImageWithExif(img, m_current->path, m_current->path, t);
    });
    connect(m_view, &ImageWidget::fliped, [this](bool x, bool y) {
        const QTransform t = QTransform().scale(x ? -1 : 1, y ? -1 : 1);
        QImage img = m_view->image().transformed(t);
        utils::saveImageWithExif(img, m_current->path, m_current->path, t);
    });
    connect(m_view, &ImageWidget::transformChanged, [this](){
        // TODO: check user settings
        if (!m_nav->isAlwaysHidden())
            m_nav->setVisible(!m_view->isWholeImageVisible());
        m_nav->setRectInImage(m_view->visibleImageRect());
    });
    connect(m_view, &ImageWidget::doubleClicked,
            this, &ViewPanel::toggleFullScreen);
    connect(m_nav, &NavigationWidget::requestMove, [this](int x, int y){
        m_view->setImageMove(x, y);
    });
    connect(this, &ViewPanel::customContextMenuRequested, this, [=] {
        m_popupMenu->showMenu(createMenuContent());
    });
    connect(m_popupMenu, &PopupMenuManager::menuItemClicked,
            this, &ViewPanel::onMenuItemClicked);
}

QWidget *ViewPanel::toolbarBottomContent()
{
    QWidget *w = new QWidget();
    QHBoxLayout *hb = new QHBoxLayout();
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(10);
    w->setLayout(hb);
    DImageButton *btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/info_normal.png");
    btn->setHoverPic(":/images/resources/images/info_hover.png");
    btn->setPressPic(":/images/resources/images/info_active.png");
    hb->addWidget(btn);
    hb->addStretch();
    connect(btn, &DImageButton::clicked,
            m_signalManager, &SignalManager::showExtensionPanel);

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/collect_normal.png");
    btn->setHoverPic(":/images/resources/images/collect_hover.png");
    btn->setPressPic(":/images/resources/images/collect_active.png");
    hb->addWidget(btn);

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/previous_normal.png");
    btn->setHoverPic(":/images/resources/images/previous_hover.png");
    btn->setPressPic(":/images/resources/images/previous_press.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, [this]() {
        m_slide->stop();
        if (m_current == m_infos.cbegin())
            return;
        --m_current;
        openImage(m_current->path);
    });

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/slideshow_normal.png");
    btn->setHoverPic(":/images/resources/images/slideshow_hover.png");
    btn->setPressPic(":/images/resources/images/slideshow_press.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, [this](){
        if (m_slide->isRunning()) {
            m_slide->stop();
            return;
        }
        QStringList paths;
        foreach (const DatabaseManager::ImageInfo& info, m_infos) {
            paths << info.path;
        }
        m_slide->setImagePaths(paths);
        m_slide->setCurrentImage(m_current->path);
        m_slide->start();
    });

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/next_normal.png");
    btn->setHoverPic(":/images/resources/images/next_hover.png");
    btn->setPressPic(":/images/resources/images/next_press.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, [this]() {
        m_slide->stop();
        if (m_current == m_infos.cend())
            return;
        ++m_current;
        if (m_current == m_infos.cend()) {
            --m_current;
            return;
        }
        openImage(m_current->path);
    });

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/edit_normal.png");
    btn->setHoverPic(":/images/resources/images/edit_hover.png");
    btn->setPressPic(":/images/resources/images/edit_press.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, [this](){
        Q_EMIT m_signalManager->editImage(m_current->path);
    });

    hb->addStretch();

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/delete_normal.png");
    btn->setHoverPic(":/images/resources/images/delete_hover.png");
    btn->setPressPic(":/images/resources/images/delete_press.png");
    hb->addWidget(btn);
    return w;
}

QWidget *ViewPanel::toolbarTopLeftContent()
{
    QWidget *w = new QWidget();
    QHBoxLayout *hb = new QHBoxLayout();
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(0);
    w->setLayout(hb);
    DImageButton *btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/album_normal.png");
    btn->setHoverPic(":/images/resources/images/album_hover.png");
    btn->setPressPic(":/images/resources/images/album_active.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked,
            m_signalManager, &SignalManager::backToMainWindow);
    return w;
}

QWidget *ViewPanel::toolbarTopMiddleContent()
{
    QWidget *w = new QWidget();
    QHBoxLayout *hb = new QHBoxLayout();
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(10);
    w->setLayout(hb);
    hb->addStretch();
    DImageButton *btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/contrarotate_normal.png");
    btn->setHoverPic(":/images/resources/images/contrarotate_hover.png");
    btn->setPressPic(":/images/resources/images/contrarotate_press.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, m_view, &ImageWidget::rotateClockWise);

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/clockwise_rotation_normal.png");
    btn->setHoverPic(":/images/resources/images/clockwise_rotation_hover.png");
    btn->setPressPic(":/images/resources/images/clockwise_rotation_press.png");
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, m_view, &ImageWidget::rotateAntiClockWise);

    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/adapt_image_normal.png");
    btn->setHoverPic(":/images/resources/images/adapt_image_hover.png");
    btn->setPressPic(":/images/resources/images/adapt_image_active.png");
    btn->setToolTip(tr("1:1 Size"));
    hb->addWidget(btn);
    connect(btn, &DImageButton::clicked, [this](){
        m_view->resetTransform();
        m_view->setScaleValue(1);
    });
    //
#if 0
    btn = new DImageButton();
    btn->setNormalPic(":/images/resources/images/share_normal.png");
    btn->setHoverPic(":/images/resources/images/share_hover.png");
    btn->setPressPic(":/images/resources/images/share_active.png");
    hb->addWidget(btn);
#endif
    hb->addStretch();
    return w;
}

QWidget *ViewPanel::extensionPanelContent()
{
    m_info = new ImageInfoWidget();
    m_info->setImagePath(m_current->path);
    return m_info;
}

void ViewPanel::resizeEvent(QResizeEvent *e)
{
    m_nav->move(e->size().width() - m_nav->width() - 10,
                e->size().height() - m_nav->height() -10);
    m_slide->setFrameSize(e->size().width(), e->size().height());
}

void ViewPanel::toggleFullScreen()
{
    if (window()->isFullScreen()) {
        window()->showNormal();
        Q_EMIT m_signalManager->showBottomToolbar();
        Q_EMIT m_signalManager->showTopToolbar();
    } else {
        // Full screen then hide bars because hide animation depends on height()
        window()->showFullScreen();
        Q_EMIT m_signalManager->hideBottomToolbar();
        Q_EMIT m_signalManager->hideExtensionPanel();
        Q_EMIT m_signalManager->hideTopToolbar();
    }
}

QString ViewPanel::createMenuContent()
{
    QJsonArray items;
    items.append(createMenuItem(IdFullScreen, tr("Fullscreen"),
                                false, "Ctrl+Alt+F"));
    items.append(createMenuItem(IdStartSlideShow, tr("Start slide show"),
                                false, "Ctrl+Alt+P"));
    const QJsonObject objF = createAlbumMenuObj(false);
    if (! objF.isEmpty()) {
        items.append(createMenuItem(IdAddToAlbum, tr("Add to album"),
                                    false, "", objF));
    }

    items.append(createMenuItem(IdSeparator, "", true));

//    items.append(createMenuItem(IdCopy, tr("Export"), false, "Ctrl+C"));
    items.append(createMenuItem(IdCopy, tr("Copy"), false, "Ctrl+C"));
    items.append(createMenuItem(IdDelete, tr("Delete"), false, "Ctrl+Delete"));
    const QJsonObject objT = createAlbumMenuObj(true);
    if (! objT.isEmpty()) {
        items.append(createMenuItem(IdRemoveFromAlbum, tr("Remove from album"),
                                    false, "", objT));
    }

    items.append(createMenuItem(IdSeparator, "", true));

    items.append(createMenuItem(IdEdit, tr("Edit")));
    items.append(createMenuItem(IdAddToFavorites, tr("Add to favorites"),
                                false, "/"));

    items.append(createMenuItem(IdSeparator, "", true));
    if (!m_view->isWholeImageVisible() && m_nav->isAlwaysHidden()) {
        items.append(createMenuItem(IdShowNavigationWindow,
                                    tr("Show navigation window")));
    } else if (!m_view->isWholeImageVisible() && !m_nav->isAlwaysHidden()) {
        items.append(createMenuItem(IdHideNavigationWindow,
                                    tr("Hide navigation window")));
    }
//    items.append(createMenuItem(IdRotateClockwise,
//                                tr("Hide navigation window")));

    items.append(createMenuItem(IdSeparator, "", true));

    items.append(createMenuItem(IdRotateClockwise, tr("Rotate clockwise"),
                                false, "Ctrl+R"));
    items.append(createMenuItem(IdRotateCounterclockwise,
                                tr("Rotate counterclockwise"),
                                false, "Ctrl+L"));

    items.append(createMenuItem(IdSeparator, "", true));

    items.append(createMenuItem(IdLabel, tr("Text tag")));
    items.append(createMenuItem(IdSetAsWallpaper, tr("Set as wallpaper")));
    items.append(createMenuItem(IdDisplayInFileManager,
                                tr("Display in file manager")));
    items.append(createMenuItem(IdImageInfo, tr("Image info"),
                                false, "Ctrl+I"));

    QJsonObject contentObj;
    contentObj["x"] = 0;
    contentObj["y"] = 0;
    contentObj["items"] = QJsonValue(items);

    QJsonDocument document(contentObj);

    return QString(document.toJson());
}

QJsonObject ViewPanel::createAlbumMenuObj(bool isRemove)
{
    const QStringList albums = DatabaseManager::instance()->getAlbumNameList();

    QJsonArray items;
    for (QString album : albums) {
        if (album == "My favorites" || album == "Recent imported") {
            continue;
        }
        const QStringList names = m_dbManager->getImageNamesByAlbum(album);
        if (isRemove) {
            if (names.indexOf(m_current->name) != -1) {
                album = tr("Remove from <<%1>>").arg(album);
                items.append(createMenuItem(IdRemoveFromAlbum, album));
            }
        }
        else {
            if (names.indexOf(m_current->name) == -1) {
                items.append(createMenuItem(IdAddToAlbum, album));
            }
        }
    }

    QJsonObject contentObj;
    if (! items.isEmpty()) {
        contentObj[""] = QJsonValue(items);
    }

    return contentObj;
}

QJsonValue ViewPanel::createMenuItem(const ViewPanel::MenuItemId id,
                                     const QString &text,
                                     const bool isSeparator,
                                     const QString &shortcut,
                                     const QJsonObject &subMenu)
{
    return QJsonValue(m_popupMenu->createItemObj(id,
                                                 text,
                                                 isSeparator,
                                                 shortcut,
                                                 subMenu));
}

void ViewPanel::onMenuItemClicked(int menuId, const QString &text)
{
    const QStringList mtl = text.split(SHORTCUT_SPLIT_FLAG);
    QString albumName = mtl.isEmpty() ? "" : mtl.first();

    switch (MenuItemId(menuId)) {
    case IdFullScreen:
        break;
    case IdStartSlideShow:
        break;
    case IdAddToAlbum:
        m_dbManager->insertImageIntoAlbum(albumName, m_current->name,
                                     m_current->time.toString(DATETIME_FORMAT));
        break;
//    case IdExport:
//        break;
    case IdCopy:
        break;
    case IdDelete:
        break;
    case IdRemoveFromAlbum:
        m_dbManager->removeImageFromAlbum(
                    albumName.replace(QRegularExpression("^.*<<"), "")
                    .replace(QRegularExpression(">>.*$"), ""),
                    m_current->name);
        break;
    case IdEdit:
        m_signalManager->editImage(m_view->imagePath());
        break;
    case IdAddToFavorites:
        m_dbManager->insertImageIntoAlbum("My favorites", m_current->name,
                                     m_current->time.toString(DATETIME_FORMAT));
        break;
    case IdRemoveFromFavorites:
        break;
    case IdShowNavigationWindow:
        m_nav->setAlwaysHidden(false);
        break;
    case IdHideNavigationWindow:
        m_nav->setAlwaysHidden(true);
        break;
    case IdRotateClockwise:
        break;
    case IdRotateCounterclockwise:
        break;
    case IdLabel:
        break;
    case IdSetAsWallpaper:
        break;
    case IdDisplayInFileManager:
        break;
    case IdImageInfo:
        break;
    default:
        break;
    }
}

void ViewPanel::openImage(const QString &path)
{
    Q_EMIT m_signalManager->gotoPanel(this);
    Q_EMIT m_signalManager->updateBottomToolbarContent(
                toolbarBottomContent(), true);
    m_view->setImage(path);
    m_nav->setImage(m_view->image());
    qDebug() << "view path: " << m_view->imagePath();
    if (m_info) {
        m_info->setImagePath(path);
    }
}