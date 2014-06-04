
/*
    Copyright (C) 2012  Spatial Transcriptomics AB,
    read LICENSE for licensing terms.
    Contact : Jose Fernandez Navarro <jose.fernandez.navarro@scilifelab.se>

*/

#include "CellViewPage.h"

#include <QDebug>

#include <QMetaObject>
#include <QMetaMethod>
#include <QString>
#include <QSortFilterProxyModel>
#include <QPrintDialog>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QColorDialog>
#include <QImageReader>
#include <QPainter>
#include <QScrollArea>
#include <QRubberBand>
#include <QStyleFactory>

#include <qtcolorpicker.h>

#include "error/Error.h"

#include "network/DownloadManager.h"

#include "CellViewPageToolBar.h"

#include "io/GeneXMLExporter.h"
#include "io/GeneTXTExporter.h"

#include "utils/Utils.h"
#include "dialogs/SelectionDialog.h"

#include "viewOpenGL/CellGLView.h"
#include "viewOpenGL/ImageTextureGL.h"
#include "viewOpenGL/GridRendererGL.h"
#include "viewOpenGL/HeatMapLegendGL.h"
#include "viewOpenGL/MiniMapGL.h"
#include "viewOpenGL/GeneRendererGL.h"

#include "model/GeneSelectionItemModel.h"
#include "model/GeneFeatureItemModel.h"

#include "CreateColorPickerPopup.h"
#include "ui_cellview.h"


CellViewPage::CellViewPage(QWidget *parent)
    : Page(parent), m_geneModel(nullptr), m_geneSelectionModel(nullptr),
      m_minimap(nullptr), m_legend(nullptr), m_gene_plotter(nullptr),
      m_image(nullptr), m_grid(nullptr), m_view(nullptr), selectionDialog(nullptr), 
      m_colorDialogGenes(nullptr), m_colorDialogGrid(nullptr)
{
    onInit();
}

CellViewPage::~CellViewPage()
{
    if (m_colorDialogGrid) {
        delete m_colorDialogGrid;
    }
    m_colorDialogGrid = 0;

    if (m_colorDialogGenes) {
        delete m_colorDialogGenes;
    }
    m_colorDialogGenes = 0;

    if (m_toolBar) {
        delete m_toolBar;
    }
    m_toolBar = 0;

    delete ui;
}

void CellViewPage::onInit()
{
    // create models (TODO: we should move the creation of the models out of the GUI)
    m_geneModel = new GeneFeatureItemModel(this);
    m_geneSelectionModel = new GeneSelectionItemModel(this);

    //create UI objects
    ui = new Ui::CellView;
    ui->setupUi(this);

    ui->clearSelection->setIcon(QIcon(QStringLiteral(":/images/clear2.png")));
    ui->saveSelection->setIcon(QIcon(QStringLiteral(":/images/file_export.png")));
    ui->exportSelection->setIcon(QIcon(QStringLiteral(":/images/export.png")));

    //gene search displays a clear button
    ui->lineEdit->setClearButtonEnabled(true);
    ui->geneSelectionFilterLineEdit->setClearButtonEnabled(true);

    // color dialogs
    m_colorDialogGrid = new QColorDialog(Globals::DEFAULT_COLOR_GRID);
     //OSX native color dialog gives problems
    m_colorDialogGrid->setOption(QColorDialog::DontUseNativeDialog, true);
    m_colorDialogGenes = new QColorDialog(Globals::DEFAULT_COLOR_GENE);
    //OSX native color dialog gives problems
    m_colorDialogGenes->setOption(QColorDialog::DontUseNativeDialog, true);

    // selection dialog
    selectionDialog = new SelectionDialog(this);

    //create tool bar and add it
    createToolBar();

    // init OpenGL graphical objects
    initGLView();

    // set the table view models
    ui->genes_tableview->setGeneFeatureItemModel(m_geneModel);
    ui->selections_tableview->setGeneSelectionItemModel(m_geneSelectionModel);

    //create connections
    createConnections();

    // setup menu buttons
    ui->selectionMenuButton->init(ui->genes_tableview);
    ui->actionMenuButton->init(this);

    //create OpenGL connections
    createGLConnections();
}

void CellViewPage::onEnter()
{
    loadData();

    DataProxy* dataProxy = DataProxy::getInstance();

    DataProxy::DatasetPtr dataset =
            dataProxy->getDatasetById(dataProxy->getSelectedDataset());
    Q_ASSERT(dataset);

    DataProxy::ChipPtr currentChip = dataProxy->getChip(dataset->chipId());
    Q_ASSERT(currentChip);

    DataProxy::DatasetStatisticsPtr statistics =
            dataProxy->getStatistics(dataProxy->getSelectedDataset());
    Q_ASSERT(statistics);

    const QTransform alignment = dataset->alignment();

    const qreal min = statistics->minValue(); //1st quantile
    const qreal max = statistics->maxValue(); // 2nd quantile;
    const qreal pooledMin = statistics->pooledMin();
    const qreal pooledMax = statistics->pooledMax();
    //const qreal sum = statistics->sum(); // total sum

    const QRectF chip_rect = QRectF(
                QPointF(currentChip->x1(), currentChip->y1()),
                QPointF(currentChip->x2(), currentChip->y2())
                );
    const QRectF chip_border = QRectF(
                QPointF(currentChip->x1Border(), currentChip->y1Border()),
                QPointF(currentChip->x2Border(), currentChip->y2Border())
                );

    // updade grid size and data
    m_grid->clearData();
    m_grid->setDimensions(chip_border, chip_rect);
    m_grid->generateData();
    m_grid->setTransform(alignment);

    // update gene size an data
    m_gene_plotter->clearData();
    m_gene_plotter->setDimensions(chip_border);
    m_gene_plotter->generateData();
    m_gene_plotter->setTransform(alignment);
    m_gene_plotter->setHitCount(min, max, pooledMin, pooledMax);

    // updated legend size and data
    m_legend->setBoundaries(min, max);

    // load cell tissue
    slotLoadCellFigure();

    // reset main variabless
    resetActionStates();
}

void CellViewPage::onExit()
{
    ui->lineEdit->clearFocus();
    ui->geneSelectionFilterLineEdit->clearFocus();
    ui->genes_tableview->clearFocus();
    /*
    ui->showAllGenes->clearFocus();
    ui->hideAllGenes->clearFocus();
    */
    ui->selections_tableview->clearFocus();
    ui->clearSelection->clearFocus();
    ui->saveSelection->clearFocus();
    ui->genes_tableview->clearSelection();
    ui->selections_tableview->clearSelection();

    m_gene_plotter->clearData();
    m_grid->clearData();
    //m_legend->clearData();
    //m_minimap->clearData();
    m_image->clear();
}

void CellViewPage::loadData()
{
    DataProxy *dataProxy = DataProxy::getInstance();
    DataProxy::DatasetPtr dataset =
            dataProxy->getDatasetById(dataProxy->getSelectedDataset());

    if (dataset.isNull()) {
        Error *error = new Error("Cell View Error", "The current selected dataset is not valid.", this);
        emit signalError(error);
        return;
    }

    //load the data
    setWaiting(true);

    //load cell tissue blue
    {
        async::DataRequest request =
                dataProxy->loadCellTissueByName(dataset->figureBlue());
        if (request.return_code() == async::DataRequest::CodeError
                || request.return_code() == async::DataRequest::CodeAbort) {
            Error *error =
                    new Error("Data loading Error", "Error loading the cell tissue image.", this);
            setWaiting(false);
            emit signalError(error);
            return;
        }
    }
    //load cell tissue red
    {
        async::DataRequest request =
                dataProxy->loadCellTissueByName(dataset->figureRed());
        if (request.return_code() == async::DataRequest::CodeError
                || request.return_code() == async::DataRequest::CodeAbort) {
            Error *error =
                    new Error("Data loading Error", "Error loading the cell tissue image.", this);
            setWaiting(false);
            emit signalError(error);
            return;
        }
    }
    //load features
    {
        async::DataRequest request =
                dataProxy->loadFeatureByDatasetId(dataset->id());
        if (request.return_code() == async::DataRequest::CodeError
                || request.return_code() == async::DataRequest::CodeAbort) {
            Error *error =
                    new Error("Data loading Error", "Error loading the cell tissue image.", this);
            setWaiting(false);
            emit signalError(error);
            return;
        }
    }
    //load dataset statistics
    {
        async::DataRequest request =
                dataProxy->loadDatasetStatisticsByDatasetId(dataset->id());
        if (request.return_code() == async::DataRequest::CodeError
                || request.return_code() == async::DataRequest::CodeAbort) {
            Error *error =
                    new Error("Data loading Error", "Error loading the dataset statistics.", this);
            setWaiting(false);
            emit signalError(error);
            return;
        }
    }
    //load genes
    {
        async::DataRequest request =
                dataProxy->loadGenesByDatasetId(dataset->id());
        if (request.return_code() == async::DataRequest::CodeError
                || request.return_code() == async::DataRequest::CodeAbort) {
            Error *error = new Error("Data loading Error", "Error loading the genes.", this);
            setWaiting(false);
            emit signalError(error);
            return;
        }
    }
    //load chip
    {
        async::DataRequest request =
                dataProxy->loadChipById(dataset->chipId());
        if (request.return_code() == async::DataRequest::CodeError
                || request.return_code() == async::DataRequest::CodeAbort) {
            Error *error = new Error("Data loading Error", "Error loading the chip array.", this);
            setWaiting(false);
            emit signalError(error);
            return;
        }
    }
    //succes downloading the data
    setWaiting(false);
}

void CellViewPage::createConnections()
{
    // go back signal
    connect(m_toolBar->m_actionNavigate_goBack,
            SIGNAL(triggered(bool)), this, SIGNAL(moveToPreviousPage()));

    // gene model signals

    connect(ui->lineEdit, SIGNAL(textChanged(QString)), ui->genes_tableview,
            SLOT(setGeneNameFilter(QString)));
    connect(ui->geneSelectionFilterLineEdit, SIGNAL(textChanged(QString)), ui->selections_tableview,
            SLOT(setGeneNameFilter(QString)));

    /*
    connect(ui->showAllGenes, SIGNAL(clicked(bool)), this,
            SLOT(slotShowAllGenes(bool)));
    connect(ui->hideAllGenes, SIGNAL(clicked(bool)), this,
            SLOT(slotHideAllGenes(bool)));
    */
    connect(m_colorDialogGenes, SIGNAL(colorSelected(QColor)), m_geneModel,
            SLOT(setColorGenes(const QColor&)));

    // cell tissue
    connect(m_toolBar->m_actionShow_cellTissueBlue,
            SIGNAL(triggered(bool)), this, SLOT(slotLoadCellFigure()));
    connect(m_toolBar->m_actionShow_cellTissueRed,
            SIGNAL(triggered(bool)), this, SLOT(slotLoadCellFigure()));

    // graphic view signals
    connect(m_toolBar->m_actionZoom_zoomIn,
            SIGNAL(triggered(bool)), m_view, SLOT(zoomIn()));
    connect(m_toolBar->m_actionZoom_zoomOut,
            SIGNAL(triggered(bool)), m_view, SLOT(zoomOut()));
    connect(m_toolBar, SIGNAL(rotateView(qreal)), m_view, SLOT(rotate(qreal)));

    // print canvas
    connect(m_toolBar->m_actionSave_save,
            SIGNAL(triggered(bool)), this, SLOT(slotSaveImage()));
    connect(m_toolBar->m_actionSave_print,
            SIGNAL(triggered(bool)), this, SLOT(slotPrintImage()));

    // export
    connect(ui->exportSelection, SIGNAL(clicked(bool)),
            this, SLOT(slotExportSelection()));

    // selection mode
    connect(m_toolBar->m_actionActivateSelectionMode,
            SIGNAL(triggered(bool)), m_view, SLOT(setSelectionMode(bool)));
    connect(m_toolBar->m_actionSelection_showSelectionDialog,
            SIGNAL(triggered(bool)), this, SLOT(slotSelectByRegExp()));

    //color selectors
    connect(m_toolBar->m_actionColor_selectColorGenes,
            SIGNAL(triggered(bool)), this, SLOT(slotLoadColor()));
    connect(m_toolBar->m_actionColor_selectColorGrid,
            SIGNAL(triggered(bool)), this, SLOT(slotLoadColor()));
}

void CellViewPage::resetActionStates()
{
    // reset gene model data
    m_geneModel->loadGenes();

    // reset gene colors
    m_geneModel->setColorGenes(Globals::DEFAULT_COLOR_GENE);

    // reset gene selection model data
    m_geneSelectionModel->reset();

    // reset color dialogs
    m_colorDialogGenes->setCurrentColor(Globals::DEFAULT_COLOR_GENE);
    m_colorDialogGrid->setCurrentColor(Globals::DEFAULT_COLOR_GRID);

    // reset cell image to show
    m_image->setVisible(true);
    m_image->setAnchor(Globals::DEFAULT_ANCHOR_IMAGE);

    // reset gene grid to not show
    m_grid->setVisible(false);
    m_grid->setAnchor(Globals::DEFAULT_ANCHOR_GRID);

    // reset gene plotter to visible
    m_gene_plotter->setVisible(true);
    m_gene_plotter->setAnchor(Globals::DEFAULT_ANCHOR_GENE);

    // reset minimap to visible true
    m_minimap->setVisible(true);
    m_minimap->setAnchor(Globals::DEFAULT_ANCHOR_MINIMAP);

    // reset legend to visible true
    m_legend->setVisible(false);
    m_legend->setAnchor(Globals::DEFAULT_ANCHOR_LEGEND);

    // reset tool bar actions
    m_toolBar->resetActions();

    DataProxy *dataProxy = DataProxy::getInstance();
    // restrict interface
    DataProxy::UserPtr current_user = dataProxy->getUser();
    m_toolBar->m_actionGroup_cellTissue->setVisible(
                (current_user->role() == Globals::ROLE_CM));
}

void CellViewPage::createToolBar()
{
    m_toolBar = new CellViewPageToolBar();
    // add tool bar to the layout
    ui->pageLayout->insertWidget(0, m_toolBar);
}

void CellViewPage::initGLView()
{
    //ui->area contains the openGL window
    m_view = ui->area->cellGlView();
    // Adding a stretch to make the opengl window occupy more space
    ui->selectionLayout->setStretch(1,10);

    // image texture graphical object
    m_image = new ImageTextureGL(this);
    m_image->setAnchor(Globals::DEFAULT_ANCHOR_IMAGE);
    m_view->addRenderingNode(m_image);

    // grid graphical object
    m_grid = new GridRendererGL(this);
    m_grid->setAnchor(Globals::DEFAULT_ANCHOR_GRID);
    m_view->addRenderingNode(m_grid);

    // gene plotter component
    m_gene_plotter = new GeneRendererGL(this);
    m_gene_plotter->setAnchor(Globals::DEFAULT_ANCHOR_GENE);
    m_view->addRenderingNode(m_gene_plotter);

    // heatmap component
    m_legend = new HeatMapLegendGL(this);
    m_legend->setAnchor(Globals::DEFAULT_ANCHOR_LEGEND);
    m_view->addRenderingNode(m_legend);

    // minimap component
    m_minimap = new MiniMapGL(this);
    m_minimap->setAnchor(Globals::DEFAULT_ANCHOR_MINIMAP);
    m_view->addRenderingNode(m_minimap);
    // minimap needs to be notified when the canvas is resized and when the image
    // is zoomed or moved
    connect(m_minimap, SIGNAL(signalCenterOn(QPointF)),
            m_view, SLOT(centerOn(QPointF)));
    connect(m_view, SIGNAL(signalSceneUpdated(QRectF)),
            m_minimap, SLOT(setScene(QRectF)));
    connect(m_view, SIGNAL(signalViewPortUpdated(QRectF)),
            m_minimap, SLOT(setViewPort(QRectF)));
    connect(m_view, SIGNAL(signalSceneTransformationsUpdated(const QTransform)),
            m_minimap, SLOT(setParentSceneTransformations(const QTransform)));
}

void CellViewPage::createGLConnections()
{

    //connect gene list model to gene plotter
    connect(m_geneModel, SIGNAL(signalSelectionChanged(DataProxy::GeneList)),
            m_gene_plotter,
            SLOT(updateSelection(DataProxy::GeneList)));
    connect(m_geneModel, SIGNAL(signalColorChanged(DataProxy::GeneList)),
            m_gene_plotter,
            SLOT(updateColor(DataProxy::GeneList)));
    connect(m_colorDialogGenes, SIGNAL(colorSelected(QColor)),
            m_gene_plotter, SLOT(updateAllColor(QColor)));

    //connect gene plotter to gene selection model
    connect(m_gene_plotter, SIGNAL(selectionUpdated()),
            this, SLOT(slotSelectionUpdated()));

    // selection actions
    connect(ui->clearSelection, SIGNAL(clicked(bool)),
            m_gene_plotter, SLOT(clearSelection()) );

    //threshold slider signal
    connect(m_toolBar, SIGNAL(thresholdLowerValueChanged(int)),
            m_gene_plotter, SLOT(setLowerLimit(int)));
    connect(m_toolBar, SIGNAL(thresholdUpperValueChanged(int)),
            m_gene_plotter, SLOT(setUpperLimit(int)));
    
    //gene attributes signals
    connect(m_toolBar, SIGNAL(intensityValueChanged(qreal)),
            m_gene_plotter, SLOT(setIntensity(qreal)));
    connect(m_toolBar, SIGNAL(sizeValueChanged(qreal)),
            m_gene_plotter, SLOT(setSize(qreal)));
    connect(m_toolBar, SIGNAL(shapeIndexChanged(Globals::GeneShape)),
            m_gene_plotter, SLOT(setShape(Globals::GeneShape)));
    connect(m_toolBar, SIGNAL(shineValueChanged(qreal)),
            m_gene_plotter, SLOT(setShine(qreal)));

    //show/not genes signal
    connect(m_toolBar->m_actionShow_showGenes,
            SIGNAL(triggered(bool)), m_gene_plotter, SLOT(setVisible(bool)));

    //visual mode signal
    connect(m_toolBar->m_actionGroup_toggleVisualMode, SIGNAL(triggered(QAction*)), this,
            SLOT(slotSetGeneVisualMode(QAction*)));

    // grid signals
    connect(m_colorDialogGrid, SIGNAL(colorSelected(const QColor&)),
            m_grid, SLOT(setColor(const QColor&)));
    connect(m_toolBar->m_actionShow_showGrid, SIGNAL(triggered(bool)),
            m_grid, SLOT(setVisible(bool)));

    // cell tissue canvas
    connect(m_toolBar->m_actionShow_showCellTissue, SIGNAL(triggered(bool)),
            m_image, SLOT(setVisible(bool)));
    connect(m_toolBar, SIGNAL(brightnessValueChanged(qreal)),
            m_image, SLOT(setIntensity(qreal)));

    // legend signals
    connect(m_toolBar->m_actionShow_showLegend, SIGNAL(toggled(bool)),
            m_legend, SLOT(setVisible(bool)));
    connect(m_toolBar->m_actionGroup_toggleLegendPosition,
            SIGNAL(triggered(QAction*)), this,
            SLOT(slotSetLegendAnchor(QAction*)));

    // minimap signals
    connect(m_toolBar->m_actionShow_showMiniMap, SIGNAL(toggled(bool)),
            m_minimap, SLOT(setVisible(bool)));
    connect(m_toolBar->m_actionGroup_toggleMinimapPosition,
            SIGNAL(triggered(QAction*)), this,
            SLOT(slotSetMiniMapAnchor(QAction*)));

    // connect threshold slider to the heatmap
    connect(m_toolBar, SIGNAL(thresholdLowerValueChanged(int)),
            m_legend, SLOT(setLowerLimit(int)));
    connect(m_toolBar, SIGNAL(thresholdUpperValueChanged(int)),
            m_legend, SLOT(setUpperLimit(int)));
}

void CellViewPage::slotLoadCellFigure()
{
    DataProxy* dataProxy = DataProxy::getInstance();
    DataProxy::UserPtr current_user = dataProxy->getUser();
    DataProxy::DatasetPtr dataset = dataProxy->getDatasetById(dataProxy->getSelectedDataset());
    Q_ASSERT(!current_user.isNull() && !dataset.isNull());

    const bool forceRedFigure = (QObject::sender() == m_toolBar->m_actionShow_cellTissueRed);
    const bool forceBlueFigure = (QObject::sender() == m_toolBar->m_actionShow_cellTissueBlue);
    const bool defaultRedFigure =
        (current_user->role() == Globals::ROLE_CM)
        && !(dataset->figureStatus() & Dataset::Aligned);
    const bool loadRedFigure = (defaultRedFigure || forceRedFigure) && !forceBlueFigure;

    QString figureid = (loadRedFigure) ? dataset->figureRed() : dataset->figureBlue();
    QIODevice *device = dataProxy->getFigure(figureid);

    //read image (TODO check file is present or corrupted)
    QImageReader reader(device);
    const QImage image = reader.read();

    //deallocate device
    device->deleteLater();

    // add image to the texture image holder
    m_image->createTexture(image);
    m_view->setScene(image.rect());

    //update checkboxes
    m_toolBar->m_actionShow_cellTissueBlue->setChecked(!loadRedFigure);
    m_toolBar->m_actionShow_cellTissueRed->setChecked(loadRedFigure);
}

void CellViewPage::slotPrintImage()
{
    QPrinter printer;
    printer.setOrientation(QPrinter::Landscape);
    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    if (dialog->exec() != QDialog::Accepted) {
        return;
    }
    QPainter painter(&printer);
    QRect rect = painter.viewport();
    QImage image = m_view->grabPixmapGL();
    QSize size = image.size();
    size.scale(rect.size(), Qt::KeepAspectRatio);
    painter.setViewport(rect);
    painter.setWindow(image.rect());
    painter.drawImage(0, 0, image);
}

void CellViewPage::slotSaveImage()
{
    QString filename =
                       QFileDialog::getSaveFileName(this, tr("Save Image"), QDir::homePath(),
                       QString("%1;;%2").
                       arg(tr("JPEG Image Files (*.jpg *.jpeg)")).
                       arg(tr("PNG Image Files (*.png)")));
    // early out
    if (filename.isEmpty()) {
        return;
    }
    // append default extension
    QRegExp regex("^.*\\.(jpg|jpeg|png)$", Qt::CaseInsensitive);
    if (!regex.exactMatch(filename)) {
        filename.append(".jpg");
    }
    const int quality = 100; //quality format (100 max, 0 min, -1 default)
    const QString format = filename.split(".", QString::SkipEmptyParts).at(1); //get the file extension
    QImage image = m_view->grabPixmapGL();
    if (!image.save(filename, format.toStdString().c_str(), quality)) {
        QMessageBox::warning(this, tr("Save Image"), tr("Error saving image."));
    }
}

void CellViewPage::slotExportSelection()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Export File"), QDir::homePath(),
                       QString("%1;;%2").
                       arg(tr("Text Files (*.txt)")).
                       arg(tr("XML Files (*.xml)")));
    // early out
    if (filename.isEmpty()) {
        return;
    }
    // append default extension
    QRegExp regex("^.*\\.(txt|xml)$", Qt::CaseInsensitive);
    if (!regex.exactMatch(filename)) {
        filename.append(".txt");
    }

    // get selected features and extend with data
    DataProxy *dataProxy = DataProxy::getInstance();
    DataProxy::DatasetStatisticsPtr statistics = dataProxy->getStatistics(dataProxy->getSelectedDataset());
    Q_ASSERT(statistics);
    const qreal max = statistics->pooledMax();
    //TODO pooledMax is not actually a correct roof since the reads per gene are sum
    const auto& features = dataProxy->getFeatureList(dataProxy->getSelectedDataset());
    const auto& uniqueSelected = DataProxy::getUniqueGeneSelected(max, features);

    QFile textFile(filename);
    QFileInfo info(textFile);

    if (textFile.open(QFile::WriteOnly | QFile::Truncate)) {
        QObject memoryGuard;
        //TODO move intantiation to factory
        GeneExporter *exporter =
            (QString::compare(info.suffix(), "XML", Qt::CaseInsensitive) == 0) ?
            dynamic_cast<GeneExporter *>(new GeneXMLExporter(&memoryGuard)) :
            dynamic_cast<GeneExporter *>(new GeneTXTExporter(GeneTXTExporter::SimpleFull,
                                        GeneTXTExporter::TabDelimited, &memoryGuard));

        exporter->exportItem(&textFile, uniqueSelected);
    }
    textFile.close();
}

void CellViewPage::slotSetGeneVisualMode(QAction *action)
{
    QVariant variant = action->property("mode");
    if (variant.canConvert(QVariant::Int)) {
        Globals::GeneVisualMode mode = static_cast<Globals::GeneVisualMode>(variant.toInt());
        m_gene_plotter->setVisualMode(mode);
    } else {
        Q_ASSERT("[CellViewPage] Undefined gene visual mode!");
    }
}

void CellViewPage::slotSetMiniMapAnchor(QAction *action)
{
    QVariant variant = action->property("mode");
    if (variant.canConvert(QVariant::Int)) {
        Globals::Anchor mode = static_cast<Globals::Anchor>(variant.toInt());
        m_minimap->setAnchor(mode);
    } else {
        Q_ASSERT("[CellViewPage] Undefined anchor!");
    }
}

void CellViewPage::slotSetLegendAnchor(QAction *action)
{
    QVariant variant = action->property("mode");
    if (variant.canConvert(QVariant::Int)) {
        Globals::Anchor mode = static_cast<Globals::Anchor>(variant.toInt());
        m_legend->setAnchor(mode);
    } else {
        Q_ASSERT("[CellViewPage] Undefined anchor!");
    }
}

void CellViewPage::slotLoadColor()
{
    if (QObject::sender() == m_toolBar->m_actionColor_selectColorGenes) {
        m_colorDialogGenes->open();
    } else if (QObject::sender() == m_toolBar->m_actionColor_selectColorGrid) {
        m_colorDialogGrid->open();
    }
}

void CellViewPage::slotSelectByRegExp()
{
    const DataProxy::GeneList& geneList = SelectionDialog::selectGenes(this);
    m_gene_plotter->selectGenes(geneList);
}

void CellViewPage::slotSelectionUpdated()
{
    // get selected features and extend with data
    DataProxy *dataProxy = DataProxy::getInstance();
    DataProxy::DatasetStatisticsPtr statistics = dataProxy->getStatistics(dataProxy->getSelectedDataset());
    Q_ASSERT(statistics);
    const qreal max = statistics->pooledMax();
    //TODO pooledMax is not actually a correct roof since the reads per gene are sum
    const auto& features = dataProxy->getFeatureList(dataProxy->getSelectedDataset());
    const auto& uniqueSelected = DataProxy::getUniqueGeneSelected(max, features);
    m_geneSelectionModel->loadSelectedGenes(uniqueSelected);
}

void CellViewPage::setVisibilityForSelectedRows(bool visible)
{
    m_geneModel->setGeneVisibility(ui->genes_tableview->geneTableItemSelection(), visible);
}

void CellViewPage::slotShowAllSelected()
{   
    setVisibilityForSelectedRows(true);
}

void CellViewPage::slotHideAllSelected()
{
    setVisibilityForSelectedRows(false);
}

void CellViewPage::slotSetColorAllSelected(const QColor &color)
{
    m_geneModel->setGeneColor(ui->genes_tableview->geneTableItemSelection(), color);
}
