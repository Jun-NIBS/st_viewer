#include "GridRendererGL.h"

#include <QVector2D>
#include "qopengl.h"
#include "math/Common.h"

static const double GRID_LINE_SIZE = 1.0;
static const QColor DEFAULT_COLOR_GRID_BORDER = Qt::darkRed;
const QColor GridRendererGL::DEFAULT_COLOR_GRID = Qt::darkGreen;

GridRendererGL::GridRendererGL(QObject *parent)
    : GraphicItemGL(parent)
{
    setVisualOption(GraphicItemGL::Transformable, true);
    setVisualOption(GraphicItemGL::Visible, false);
    setVisualOption(GraphicItemGL::Selectable, false);
    setVisualOption(GraphicItemGL::Yinverted, false);
    setVisualOption(GraphicItemGL::Xinverted, false);
    setVisualOption(GraphicItemGL::RubberBandable, false);
}

GridRendererGL::~GridRendererGL()
{
}

void GridRendererGL::draw(QOpenGLFunctionsVersion &qopengl_functions)
{
    qopengl_functions.glEnable(GL_LINE_SMOOTH);
    {
        qopengl_functions.glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        qopengl_functions.glLineWidth(GRID_LINE_SIZE);

        qopengl_functions.glBegin(GL_LINES);
        {
            // draw borders of the array
            qopengl_functions.glColor4f(static_cast<GLfloat>(m_gridBorderColor.redF()),
                                        static_cast<GLfloat>(m_gridBorderColor.greenF()),
                                        static_cast<GLfloat>(m_gridBorderColor.blueF()),
                                        static_cast<GLfloat>(m_gridBorderColor.alphaF()));
            for (const auto indice : m_border_vertex) {
                qopengl_functions.glVertex2f(indice.x(), indice.y());
            }

            // draw the array (grid)
            qopengl_functions.glColor4f(static_cast<GLfloat>(m_gridColor.redF()),
                                        static_cast<GLfloat>(m_gridColor.greenF()),
                                        static_cast<GLfloat>(m_gridColor.blueF()),
                                        static_cast<GLfloat>(m_gridColor.alphaF()));
            for (const auto indice : m_grid_vertex) {
                qopengl_functions.glVertex2f(indice.x(), indice.y());
            }
        }

        qopengl_functions.glEnd();
    }

    qopengl_functions.glDisable(GL_LINE_SMOOTH);

    // set the color back to white to not over-draw the textures
    qopengl_functions.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void GridRendererGL::setSelectionArea(const SelectionEvent *)
{
}

void GridRendererGL::clearData()
{
    // chip grid stuff
    m_border = QRectF();
    m_rect = QRectF();
    m_gridColor = DEFAULT_COLOR_GRID;
    m_gridBorderColor = DEFAULT_COLOR_GRID_BORDER;
    m_grid_vertex.clear();
    m_border_vertex.clear();
}

void GridRendererGL::generateData()
{
    m_grid_vertex.clear();
    m_border_vertex.clear();

    // generate borders
    for (float y = m_border.top(); y <= m_border.bottom(); y += 1.0) {
        if (m_rect.top() <= y && y <= m_rect.bottom()) {
            m_border_vertex.append(QVector2D(m_border.left(), y));
            m_border_vertex.append(QVector2D(m_rect.left(), y));
            m_border_vertex.append(QVector2D(m_rect.right(), y));
            m_border_vertex.append(QVector2D(m_border.right(), y));
        } else {
            m_border_vertex.append(QVector2D(m_border.left(), y));
            m_border_vertex.append(QVector2D(m_border.right(), y));
        }
    }
    for (float x = m_border.left(); x <= m_border.right(); x += 1.0) {
        if (m_rect.left() <= x && x <= m_rect.right()) {
            m_border_vertex.append(QVector2D(x, m_border.top()));
            m_border_vertex.append(QVector2D(x, m_rect.top()));
            m_border_vertex.append(QVector2D(x, m_rect.bottom()));
            m_border_vertex.append(QVector2D(x, m_border.bottom()));
        } else {
            m_border_vertex.append(QVector2D(x, m_border.top()));
            m_border_vertex.append(QVector2D(x, m_border.bottom()));
        }
    }

    // generate grid
    for (float y = m_rect.top(); y <= m_rect.bottom(); y += GRID_LINE_SIZE) {
        m_grid_vertex.append(QVector2D(m_rect.left(), y));
        m_grid_vertex.append(QVector2D(m_rect.right(), y));
    }
    for (float x = m_rect.left(); x <= m_rect.right(); x += GRID_LINE_SIZE) {
        m_grid_vertex.append(QVector2D(x, m_rect.top()));
        m_grid_vertex.append(QVector2D(x, m_rect.bottom()));
    }

    // check boundaries
    if (!qFuzzyCompare(Math::qMod(m_rect.bottom() - m_rect.top(), GRID_LINE_SIZE), 0.0)) {
        m_grid_vertex.append(QVector2D(m_rect.left(), m_rect.bottom()));
        m_grid_vertex.append(QVector2D(m_rect.right(), m_rect.bottom()));
    }

    if (!qFuzzyCompare(Math::qMod(m_rect.right() - m_rect.left(), GRID_LINE_SIZE), 0.0)) {
        m_grid_vertex.append(QVector2D(m_rect.right(), m_rect.top()));
        m_grid_vertex.append(QVector2D(m_rect.right(), m_rect.bottom()));
    }
}

void GridRendererGL::setDimensions(const QRectF &border, const QRectF &rect)
{
    m_border = border;
    m_rect = rect;
}

const QRectF GridRendererGL::border() const
{
    return m_border;
}

const QRectF GridRendererGL::rectangle() const
{
    return m_rect;
}

void GridRendererGL::setColor(const QColor &color)
{
    if (m_gridColor != color) {
        m_gridColor = color;
        emit updated();
    }
}

const QColor GridRendererGL::color() const
{
    return m_gridColor;
}

const QRectF GridRendererGL::boundingRect() const
{
    return m_border;
}
