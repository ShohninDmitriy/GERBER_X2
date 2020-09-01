// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "gbrfile.h"
#include <QElapsedTimer>
#include <QSemaphore>
#include <QThread>
#include <project.h>
#include <settings.h>

using namespace Gerber;

static Format* crutch;

File::File(QDataStream& stream)
{
    m_itemGroup.append({ new ItemGroup, new ItemGroup });
    read(stream);
}

File::File(const QString& fileName)
{
    m_itemGroup.append({ new ItemGroup, new ItemGroup });
    m_name = fileName;
}

template <typename T>
void addData(QByteArray& dataArray, const T& data)
{
    dataArray.append(reinterpret_cast<const char*>(&data), sizeof(T));
}

File::~File() { }

const QMap<int, QSharedPointer<AbstractAperture>>* File::apertures() const { return &m_apertures; }

Paths File::merge() const
{
    QElapsedTimer t;
    t.start();
    m_mergedPaths.clear();
    int i = 0;
    while (i < size()) {
        Clipper clipper;
        clipper.AddPaths(m_mergedPaths, ptSubject, true);
        const auto exp = at(i).state().imgPolarity();
        do {
            const GraphicObject& go = at(i++);
            clipper.AddPaths(go.paths(), ptClip, true);
        } while (i < size() && exp == at(i).state().imgPolarity());
        if (at(i - 1).state().imgPolarity() == Positive)
            clipper.Execute(ctUnion, m_mergedPaths, pftPositive);
        else
            clipper.Execute(ctDifference, m_mergedPaths, pftNonZero);
    }
    if (GlobalSettings::gbrCleanPolygons())
        CleanPolygons(m_mergedPaths, 0.0005 * uScale);
    return m_mergedPaths;
}

void File::grouping(PolyNode* node, Pathss* pathss, File::Group group)
{
    Path path;
    Paths paths;
    switch (group) {
    case CutoffGroup:
        if (!node->IsHole()) {
            path = node->Contour;
            paths.push_back(path);
            for (int var = 0; var < node->ChildCount(); ++var) {
                path = node->Childs[var]->Contour;
                paths.push_back(path);
            }
            pathss->push_back(paths);
        }
        for (int var = 0; var < node->ChildCount(); ++var) {
            grouping(node->Childs[var], pathss, group);
        }
        break;
    case CopperGroup:
        if (node->IsHole()) {
            path = node->Contour;
            paths.push_back(path);
            for (int var = 0; var < node->ChildCount(); ++var) {
                path = node->Childs[var]->Contour;
                paths.push_back(path);
            }
            pathss->push_back(paths);
        }
        for (int var = 0; var < node->ChildCount(); ++var) {
            grouping(node->Childs[var], pathss, group);
        }
        break;
    }
}

File::ItemsType File::itemsType() const { return m_itemsType; }

ItemGroup* File::itemGroup(ItemsType type) const { return m_itemGroup[type]; }

Pathss& File::groupedPaths(File::Group group, bool fl)
{
    if (m_groupedPaths.isEmpty()) {
        PolyTree polyTree;
        Clipper clipper;
        clipper.AddPaths(mergedPaths(), ptSubject, true);
        IntRect r(clipper.GetBounds());
        int k = /*uScale*/ 1;
        Path outer = {
            IntPoint(r.left - k, r.bottom + k),
            IntPoint(r.right + k, r.bottom + k),
            IntPoint(r.right + k, r.top - k),
            IntPoint(r.left - k, r.top - k)
        };
        if (fl)
            ReversePath(outer);
        clipper.AddPath(outer, ptSubject, true);
        clipper.Execute(ctUnion, polyTree, pftNonZero);
        grouping(polyTree.GetFirst(), &m_groupedPaths, group);
    }
    return m_groupedPaths;
}

bool File::flashedApertures() const
{
    for (QSharedPointer<AbstractAperture> a : m_apertures) {
        if (a.data()->isFlashed())
            return true;
    }
    return false;
}

ItemGroup* File::itemGroup() const { return m_itemGroup[m_itemsType]; }

void File::addToScene() const
{
    for (const auto var : m_itemGroup) {
        var->addToScene();
        var->setZValue(-m_id);
    }
}

void File::setColor(const QColor& color)
{
    AbstractFile::setColor(color);
    m_itemGroup[Normal]->setBrush(color);
    m_itemGroup[ApPaths]->setPen(QPen(color, 0.0));
}

void File::setItemType(File::ItemsType type)
{
    if (m_itemsType == type)
        return;

    m_itemsType = type;

    m_itemGroup[Normal]->setVisible(false);
    m_itemGroup[ApPaths]->setVisible(false);
    m_itemGroup[Components]->setVisible(false);

    m_itemGroup[m_itemsType]->setVisible(true);
}

void Gerber::File::write(QDataStream& stream) const
{
    stream << *this;
    stream << m_apertures;
    stream << m_format;
    //stream << layer;
    //stream << miror;
    stream << rawIndex;
    stream << m_itemsType;
    stream << m_components;
    _write(stream);
}

void Gerber::File::read(QDataStream& stream)
{
    crutch = &m_format; ///////////////////
    stream >> *this;
    stream >> m_apertures;
    stream >> m_format;
    //stream >> layer;
    //stream >> miror;
    stream >> rawIndex;
    stream >> m_itemsType;
    stream >> m_components;
    for (GraphicObject& go : *this) {
        go.m_gFile = this;
        go.m_state.m_format = format();
    }
    _read(stream);
}

void Gerber::File::createGi()
{
    // fill copper
    for (Paths& paths : groupedPaths()) {
        GraphicsItem* item = new GerberItem(paths, this);
        m_itemGroup[Normal]->append(item);
    }

    // add components
    for (const Component& c : m_components) {
        if (!c.referencePoint.isNull())
            m_itemGroup[Components]->append(new ComponentItem(c, this));
    }

    {
        auto contains = [&](const Path& path) -> bool {
            constexpr double k = 0.001 * uScale;
            for (const Path& chPath : checkList) { // find copy
                int counter = 0;
                if (chPath.size() == path.size()) {
                    for (const IntPoint& p1 : chPath) {
                        for (const IntPoint& p2 : path) {
                            if ((abs(p1.X - p2.X) < k) && (abs(p1.Y - p2.Y) < k)) {
                                ++counter;
                                break;
                            }
                        }
                    }
                }
                if (counter == path.size())
                    return true;
            }
            return false;
        };

        for (const GraphicObject& go : *this) {
            if (!go.path().isEmpty()) {
                if (GlobalSettings::gbrSimplifyRegions() && go.path().first() == go.path().last()) {
                    Paths paths;
                    SimplifyPolygon(go.path(), paths);
                    for (Path& path : paths) {
                        path.append(path.first());
                        if (!GlobalSettings::gbrSkipDuplicates()) {
                            checkList.push_front(path);
                            m_itemGroup[ApPaths]->append(new AperturePathItem(checkList.front(), this));
                        } else if (!contains(path)) {
                            checkList.push_front(path);
                            m_itemGroup[ApPaths]->append(new AperturePathItem(checkList.front(), this));
                        }
                    }
                } else {
                    if (!GlobalSettings::gbrSkipDuplicates()) {
                        m_itemGroup[ApPaths]->append(new AperturePathItem(go.path(), this));
                    } else if (!contains(go.path())) {
                        m_itemGroup[ApPaths]->append(new AperturePathItem(go.path(), this));
                        checkList.push_front(go.path());
                    }
                }
            }
        }
    }

    if (m_itemGroup[Components]->size())
        m_itemsType = Components; ////////////////////////

    m_itemGroup[Normal]->setVisible(false);
    m_itemGroup[ApPaths]->setVisible(false);
    m_itemGroup[Components]->setVisible(false);

    m_itemGroup[m_itemsType]->setVisible(true);
}

QDebug operator<<(QDebug debug, const Gerber::State& state)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "State("
                    << "D0" << state.dCode() << ", "
                    << "G0" << state.gCode() << ", "
                    << QStringLiteral("Positive|Negative").split('|').at(state.imgPolarity()) << ", "
                    << QStringLiteral("Linear|ClockwiseCircular|CounterclockwiseCircular").split('|').at(state.interpolation() - 1) << ", "
                    << QStringLiteral("Aperture|Line|Region").split('|').at(state.type()) << ", "
                    << QStringLiteral("Undef|Single|Multi").split('|').at(state.quadrant()) << ", "
                    << QStringLiteral("Off|On").split('|').at(state.region()) << ", "
                    << QStringLiteral("NoMirroring|X_Mirroring|Y_Mirroring|XY_Mirroring").split('|').at(state.mirroring()) << ", "
                    << QStringLiteral("aperture") << state.aperture() << ", "
                    << state.curPos() << ", "
                    << QStringLiteral("scaling") << state.scaling() << ", "
                    << QStringLiteral("rotating") << state.rotating() << ", "
                    << ')';
    return debug;
}

QDataStream& operator>>(QDataStream& stream, QSharedPointer<AbstractAperture>& aperture)
{
    int type;
    stream >> type;
    switch (type) {
    case Circle:
        aperture = QSharedPointer<AbstractAperture>(new ApCircle(stream, crutch));
        break;
    case Rectangle:
        aperture = QSharedPointer<AbstractAperture>(new ApRectangle(stream, crutch));
        break;
    case Obround:
        aperture = QSharedPointer<AbstractAperture>(new ApObround(stream, crutch));
        break;
    case Polygon:
        aperture = QSharedPointer<AbstractAperture>(new ApPolygon(stream, crutch));
        break;
    case Macro:
        aperture = QSharedPointer<AbstractAperture>(new ApMacro(stream, crutch));
        break;
    case Block:
        aperture = QSharedPointer<AbstractAperture>(new ApBlock(stream, crutch));
        break;
    }
    return stream;
}
