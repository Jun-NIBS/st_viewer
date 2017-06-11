#include "SelectionSpotsWidget.h"

#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include "SettingsStyle.h"

#include "ui_spotsSelectionWidget.h"

using namespace Style;

SelectionSpotsWidget::SelectionSpotsWidget(const UserSelection::SpotListType &spots,
                                           const UserSelection::Matrix &counts,
                                           QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , m_ui(new Ui::spotsSelectionWidget())
{
    m_ui->setupUi(this);
    m_ui->searchField->setClearButtonEnabled(true);
    m_ui->searchField->setFixedSize(CELL_PAGE_SUB_MENU_LINE_EDIT_SIZE);
    m_ui->searchField->setStyleSheet(CELL_PAGE_SUB_MENU_LINE_EDIT_STYLE);

    // data model
    const int columns = 2;
    const int rows = spots.size();
    QStandardItemModel *model = new QStandardItemModel(rows,columns,this);
    model->setHorizontalHeaderItem(0, new QStandardItem(QString("Spot")));
    model->setHorizontalHeaderItem(1, new QStandardItem(QString("Count")));
    // populate
    for (uword i = 0; i < counts.n_rows; ++i) {
        const auto spot = spots.at(i);
        const QString spot_str = QString::number(spot.first) + "x" + QString::number(spot.second);
        const float count = sum(counts.row(i));
        model->setItem(i,0,new QStandardItem(spot_str));
        model->setItem(i,1,new QStandardItem(QString::number(count)));
    }
    // sorting model
    QSortFilterProxyModel *proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_ui->tableview->setModel(proxy);

    // settings for the table
    m_ui->tableview->setSortingEnabled(true);
    m_ui->tableview->setShowGrid(true);
    m_ui->tableview->setWordWrap(true);
    m_ui->tableview->setAlternatingRowColors(true);
    m_ui->tableview->sortByColumn(0, Qt::AscendingOrder);

    m_ui->tableview->setFrameShape(QFrame::StyledPanel);
    m_ui->tableview->setFrameShadow(QFrame::Sunken);
    m_ui->tableview->setGridStyle(Qt::SolidLine);
    m_ui->tableview->setCornerButtonEnabled(false);
    m_ui->tableview->setLineWidth(1);

    m_ui->tableview->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tableview->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui->tableview->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_ui->tableview->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ui->tableview->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_ui->tableview->horizontalHeader()->setSortIndicatorShown(true);
    m_ui->tableview->verticalHeader()->hide();

    m_ui->tableview->model()->submit(); // support for caching (speed up)

    // Connect the search field signal
    connect(m_ui->searchField,
            &QLineEdit::textChanged,
            proxy,
            &QSortFilterProxyModel::setFilterFixedString);
}

SelectionSpotsWidget::~SelectionSpotsWidget()
{
}


