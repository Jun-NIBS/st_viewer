#include "SpotsWidget.h"

#include <QMenu>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QPushButton>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QAction>
#include <QColorDialog>

#include "viewTables/SpotsTableView.h"
#include "model/SpotItemModel.h"
#include "utils/SetTips.h"
#include "SettingsStyle.h"
#include "SettingsVisual.h"

using namespace Style;

SpotsWidget::SpotsWidget(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(nullptr)
    , m_spots_tableview(nullptr)
    , m_colorList(nullptr)
{
    // one layout for the controls and another for the table
    QVBoxLayout *spotsLayout = new QVBoxLayout();
    spotsLayout->setSpacing(0);
    spotsLayout->setContentsMargins(10, 10, 10, 10);
    QHBoxLayout *spotListLayout = new QHBoxLayout();
    spotListLayout->setSpacing(0);
    spotListLayout->setContentsMargins(0, 5, 0, 5);

    // add separation between buttons
    spotListLayout->addSpacing(10);

    QPushButton *showSelectedButton = new QPushButton(this);
    configureButton(showSelectedButton,
                    QIcon(QStringLiteral(":/images/visible.png")),
                    tr("Show selected spots"));
    spotListLayout->addWidget(showSelectedButton);
    // add separation
    spotListLayout->addSpacing(CELL_PAGE_SUB_MENU_BUTTON_SPACE);

    QPushButton *hideSelectedButton = new QPushButton(this);
    configureButton(hideSelectedButton,
                    QIcon(QStringLiteral(":/images/nonvisible.png")),
                    tr("Hide selected spots"));
    spotListLayout->addWidget(hideSelectedButton);
    // add separation
    spotListLayout->addSpacing(CELL_PAGE_SUB_MENU_BUTTON_SPACE);

    QPushButton *selectionAllButton = new QPushButton(this);
    configureButton(selectionAllButton,
                    QIcon(QStringLiteral(":/images/select-all.png")),
                    tr("Select all spots"));
    spotListLayout->addWidget(selectionAllButton);
    // add separation
    spotListLayout->addSpacing(CELL_PAGE_SUB_MENU_BUTTON_SPACE);

    QPushButton *selectionClearAllButton = new QPushButton(this);
    configureButton(selectionClearAllButton,
                    QIcon(QStringLiteral(":/images/unselect-all.png")),
                    tr("Deselect all spots"));
    spotListLayout->addWidget(selectionClearAllButton);
    // add separation
    spotListLayout->addSpacing(CELL_PAGE_SUB_MENU_BUTTON_SPACE);

    QPushButton *showColorButton = new QPushButton(this);
    configureButton(showColorButton,
                    QIcon(QStringLiteral(":/images/select-color.png")),
                    tr("Set color of selected spots"));
    // show color button will open up a color selector
    m_colorList.reset(new QColorDialog(Visual::DEFAULT_COLOR_GENE, this));
    m_colorList->setOption(QColorDialog::DontUseNativeDialog, true);
    spotListLayout->addWidget(showColorButton);
    // add separation
    spotListLayout->addSpacing(CELL_PAGE_SUB_MENU_BUTTON_SPACE);

    m_lineEdit.reset(new QLineEdit(this));
    m_lineEdit->setClearButtonEnabled(true);
    m_lineEdit->setFixedSize(CELL_PAGE_SUB_MENU_LINE_EDIT_SIZE);
    m_lineEdit->setStyleSheet(CELL_PAGE_SUB_MENU_LINE_EDIT_STYLE);
    setToolTipAndStatusTip(tr("Search by spot name"), m_lineEdit.data());
    spotListLayout->addWidget(m_lineEdit.data());
    spotListLayout->setAlignment(m_lineEdit.data(), Qt::AlignRight);

    // add actions menu to main layout
    spotsLayout->addLayout(spotListLayout);

    // create genes table
    m_spots_tableview.reset(new SpotsTableView(this));
    // add table to main layout
    spotsLayout->addWidget(m_spots_tableview.data());

    // set main layout
    setLayout(spotsLayout);

    // connections
    connect(showSelectedButton, SIGNAL(clicked(bool)), this, SLOT(slotShowAllSelected()));
    connect(hideSelectedButton, SIGNAL(clicked(bool)), this, SLOT(slotHideAllSelected()));
    connect(selectionAllButton, SIGNAL(clicked(bool)), m_spots_tableview.data(), SLOT(selectAll()));
    connect(selectionClearAllButton,
            SIGNAL(clicked(bool)),
            m_spots_tableview.data(),
            SLOT(clearSelection()));
    connect(showColorButton, &QPushButton::clicked, [=] {
        m_colorList->show();
        m_colorList->raise();
        m_colorList->activateWindow();
    });
    connect(m_colorList.data(), &QColorDialog::colorSelected, [=]() {
        slotSetColorAllSelected(m_colorList->currentColor());
    });
    connect(m_lineEdit.data(),
            SIGNAL(textChanged(QString)),
            m_spots_tableview.data(),
            SLOT(setNameFilter(QString)));
    connect(getModel(),
            SIGNAL(signalSpotSelectionChanged()),
            this,
            SIGNAL(signalSpotsUpdated()));
    connect(getModel(),
            SIGNAL(signalSpotColorChanged()),
            this,
            SIGNAL(signalSpotsUpdated()));
}

SpotsWidget::~SpotsWidget()
{
}

void SpotsWidget::clear()
{
    m_lineEdit->clearFocus();
    m_lineEdit->clear();
    m_spots_tableview->clearSelection();
    m_spots_tableview->clearFocus();
    getModel()->clear();
    m_colorList->setCurrentColor(Visual::DEFAULT_COLOR_GENE);
}

void SpotsWidget::updateModelTable()
{
    m_spots_tableview->update();
}

void SpotsWidget::configureButton(QPushButton *button, const QIcon &icon, const QString &tooltip)
{
    Q_ASSERT(button != nullptr);
    button->setIcon(icon);
    button->setIconSize(CELL_PAGE_SUB_MENU_ICON_SIZE);
    button->setFixedSize(CELL_PAGE_SUB_MENU_BUTTON_SIZE);
    button->setStyleSheet(CELL_PAGE_SUB_MENU_BUTTON_STYLE);
    button->setCursor(Qt::PointingHandCursor);
    setToolTipAndStatusTip(tooltip, button);
}

void SpotsWidget::slotShowAllSelected()
{
    slotSetVisibilityForSelectedRows(true);
}

void SpotsWidget::slotHideAllSelected()
{
    slotSetVisibilityForSelectedRows(false);
}

void SpotsWidget::slotSetVisibilityForSelectedRows(bool visible)
{
    getModel()->setVisibility(m_spots_tableview->getItemSelection(), visible);
    m_spots_tableview->update();
}

void SpotsWidget::slotSetColorAllSelected(const QColor &color)
{
    getModel()->setColor(m_spots_tableview->getItemSelection(), color);
    m_spots_tableview->update();
}

void SpotsWidget::slotLoadDataset(const Dataset &dataset)
{
    getModel()->loadDataset(dataset);
    m_spots_tableview->update();
}

SpotItemModel *SpotsWidget::getModel()
{
    SpotItemModel *spotModel = qobject_cast<SpotItemModel *>(getProxyModel()->sourceModel());
    Q_ASSERT(spotModel);
    return spotModel;
}

QSortFilterProxyModel *SpotsWidget::getProxyModel()
{
    QSortFilterProxyModel *proxyModel
        = qobject_cast<QSortFilterProxyModel *>(m_spots_tableview->model());
    Q_ASSERT(proxyModel);
    return proxyModel;
}
