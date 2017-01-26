#include "dataModel/Feature.h"

Feature::Feature()
    : m_gene()
    , m_count(0)
    , m_x(0)
    , m_y(0)
{
}

Feature::Feature(const QString &gene, float x, float y, int count)
    : m_gene(gene)
    , m_count(count)
    , m_x(x)
    , m_y(y)
{
}

Feature::Feature(const Feature &other)
{
    m_gene = other.m_gene;
    m_count = other.m_count;
    m_x = other.m_x;
    m_y = other.m_y;
}

Feature::~Feature()
{
}

Feature &Feature::operator=(const Feature &other)
{
    m_gene = other.m_gene;
    m_count = other.m_count;
    m_x = other.m_x;
    m_y = other.m_y;
    return (*this);
}

bool Feature::operator==(const Feature &other) const
{
    return (m_gene == other.m_gene && m_count == other.m_count && m_x == other.m_x
            && m_y == other.m_y);
}

const QString Feature::gene() const
{
    return m_gene;
}

int Feature::count() const
{
    return m_count;
}

float Feature::x() const
{
    return m_x;
}

float Feature::y() const
{
    return m_y;
}

Feature::SpotType Feature::spot() const
{
    return Feature::SpotType(m_x, m_y);
}

void Feature::gene(const QString &gene)
{
    m_gene = gene;
}

void Feature::count(int count)
{
    m_count = count;
}

void Feature::x(float x)
{
    m_x = x;
}

void Feature::y(float y)
{
    m_y = y;
}
