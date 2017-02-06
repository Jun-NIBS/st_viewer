#ifndef CELLGLVIEW_H
#define CELLGLVIEW_H

#include <QOpenGLWidget>
#include <QPointer>

#include "GraphicItemGL.h"
#include "SelectionEvent.h"

#include <functional>

class GraphicItemGL;
class QRubberBand;
class RubberbandGL;

// CellGLView is a container
// to render OpenGL GraphicItemGL type objects
// It allows for zooming, panning and selection.
// It is based on a QOpenGLWidget which is an OpenGL
// renderable qt widget.
// It also makes use of QOpenGLfunctions to
// assure cross-platform compatibility

// The main concept in this class is that the rendering
// canvas is based on the size of the cell tissue image
// and that is what is called "scene" here, the "viewport"
// would be the size of the widget so these two concepts
// are used to compute zooming, scroll-bars and so
// What it is shown in the canvas is the cell tissue image
// in its original resolution and size. Then the spots(genes)
// are shown on top of the image and therefore their coordinates
// (array coordinates) must be transformed to the image pixel space.

class CellGLView : public QOpenGLWidget
{
    Q_OBJECT

public:
    enum MouseEventType { moveType, pressType, releaseType };

    explicit CellGLView(QWidget *parent = 0);
    virtual ~CellGLView();

    // add/remove nodes from the rendering queue
    void addRenderingNode(QSharedPointer<GraphicItemGL> node);
    void removeRenderingNode(QSharedPointer<GraphicItemGL> node);

    // return a QImage representation of the canvas
    const QImage grabPixmapGL();

    // clear all local variables and data
    void clearData();

    // we must keep these overrided functions public so they can
    // be accessed from the ScrollArea class which wraps around
    // this object to implement scroll bars
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    bool event(QEvent *e) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

public slots:

    // TODO slots should have the prefix "slot"

    // Zooming the canvas
    void zoomOut();
    void zoomIn();

    // slot to enable the rubberband selection mode
    void setSelectionMode(const bool selectionMode);

    // viewport is what is visible in the canvas
    // scene is the size of the tissue image
    void setViewPort(const QRectF &viewport);
    void setScene(const QRectF &scene);

protected:
    // OpenGL rendering and initialization functions
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);

    // returns the node local transformations in the view CS adjusted for anchor
    const QTransform nodeTransformations(QSharedPointer<GraphicItemGL> node) const;

signals:

private:
    // used to filter nodes for mouse events
    typedef std::function<bool(const GraphicItemGL &)> FilterFunc;

    // helper function to adjust the zoom level
    void setZoomFactorAndUpdate(const float zoom);

    // set the center point on the scene
    void setSceneFocusCenterPointWithClamping(const QPointF &center_point);

    // helper functions used to compute center position/zoom/padding
    QRectF allowedCenterPoints() const;
    float clampZoomFactorToAllowedRange(const float zoom) const;
    float minZoom() const;
    float maxZoom() const;

    // returns all the transformations applied to the scene
    const QTransform sceneTransformations() const;

    // this function ensures that the whole image fits to the canvas
    void setDefaultPanningAndZooming();

    // notify rubberbandable nodes with a rubberband event
    void sendRubberBandEventToNodes(const QRectF &rubberBand, const QMouseEvent *event);

    // returns true if the event was sent to at least one of the nodes
    bool sendMouseEventToNodes(const QPoint &point,
                               const QMouseEvent *event,
                               const MouseEventType type,
                               const FilterFunc &filterFunc);

    // scene and viewport aux variables
    QRectF m_viewport;
    QRectF m_scene;

    // list of nodes to be renderered in the view
    QList<QSharedPointer<GraphicItemGL>> m_nodes;

    // auxiliary variables for panning, zoom and selection
    QPoint m_originPanning;
    QPoint m_originRubberBand;
    bool m_panning;
    bool m_rubberBanding;
    bool m_selecting;
    QScopedPointer<RubberbandGL> m_rubberband;
    QPointF m_scene_focus_center_point;
    float m_zoom_factor;

    // scene viewport projection
    QMatrix4x4 m_projm;

    // a cross platform wrapper around OpenGL functions
    GraphicItemGL::QOpenGLFunctionsVersion m_qopengl_functions;

    Q_DISABLE_COPY(CellGLView)
};

#endif // CELLGLVIEW_H
