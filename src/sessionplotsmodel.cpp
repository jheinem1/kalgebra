// SPDX-License-Identifier: GPL-2.0-or-later
#include "sessionplotsmodel.h"

#include <QScopedValueRollback>
#include <analitza/variables.h>
#include <analitzaplot/functiongraph.h>
#include <analitzaplot/plotsfactory.h>

SessionPlotsModel::SessionPlotsModel(const QSharedPointer<Analitza::Variables> &variables, QObject *parent)
    : Analitza::PlotsModel(parent)
    , m_variables(variables)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &SessionPlotsModel::synchronize);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SessionPlotsModel::synchronize);
    connect(this, &QAbstractItemModel::dataChanged, this, &SessionPlotsModel::synchronize);
    connect(this, &QAbstractItemModel::modelReset, this, &SessionPlotsModel::synchronize);
}

SessionPlotsModel::~SessionPlotsModel()
{
    // PlotsModel clears its rows in its destructor, after our members are
    // destroyed.
    disconnect(this, nullptr, this, nullptr);
}

QString SessionPlotsModel::nextFunctionName() const
{
    int number = 0;
    QString name;
    do {
        name = QStringLiteral("f%1").arg(number++);
    } while (m_variables->contains(name));
    return name;
}

bool SessionPlotsModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (!idx.isValid() || idx.row() >= rowCount())
        return false;
    auto item = index(idx.row(), 0).data(PlotRole).value<Analitza::PlotItem *>();
    if (role == Qt::EditRole && idx.column() == 0) {
        const QString name = value.toString();
        const Analitza::Expression declaration(name + QStringLiteral(":=0"));
        if (!declaration.isCorrect() || !declaration.isDeclaration() || declaration.name() != name || (name != item->name() && m_variables->contains(name)))
            return false;
    }
    if (role == Qt::EditRole && idx.column() == 1) {
        auto builder = Analitza::PlotsFactory::self()->requestPlot(Analitza::Expression(value.toString()), item->spaceDimension(), m_variables);
        if (!builder.canDraw())
            return false;
        auto replacement = builder.create(item->color(), item->name());
        replacement->setVisible(item->isVisible());
        updatePlot(idx.row(), replacement);
        return true;
    }
    return Analitza::PlotsModel::setData(idx, value, role);
}

void SessionPlotsModel::synchronize()
{
    if (m_synchronizing)
        return;
    QScopedValueRollback<bool> guard(m_synchronizing, true);
    QHash<QString, Analitza::Expression> current;
    for (int row = 0; row < rowCount(); ++row) {
        auto item = index(row, 0).data(PlotRole).value<Analitza::PlotItem *>();
        current.insert(item->name(), item->expression());
    }
    bool changed = false;
    // Removing a plot only removes its view. Keep the shared definition available
    // to the calculator and to functions which depend on it.
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        // Cosmetic changes must not overwrite a function redefined in the
        // calculator.
        if (!m_published.contains(it.key()) || m_published.value(it.key()).toString() != it.value().toString()) {
            m_variables->modify(it.key(), it.value());
            changed = true;
        }
    }
    m_published = current;
    if (changed)
        Q_EMIT functionsChanged();
}

void SessionPlotsModel::refresh()
{
    if (m_synchronizing)
        return;
    {
        QScopedValueRollback<bool> guard(m_synchronizing, true);
        for (int row = 0; row < rowCount(); ++row) {
            auto item = index(row, 0).data(PlotRole).value<Analitza::PlotItem *>();
            Analitza::Expression source(item->display());
            const auto definition = m_variables->contains(item->name()) ? m_variables->valueExpression(item->name()) : Analitza::Expression();
            if (definition.isCorrect() && m_published.contains(item->name()) && definition.toString() != m_published.value(item->name()).toString())
                source = definition;
            auto builder = Analitza::PlotsFactory::self()->requestPlot(source, item->spaceDimension(), m_variables);
            if (!builder.canDraw())
                continue;
            auto replacement = builder.create(item->color(), item->name());
            replacement->setVisible(item->isVisible());
            if (auto graph = dynamic_cast<Analitza::FunctionGraph *>(item)) {
                for (const auto &parameter : graph->parameters()) {
                    if (graph->hasIntervals()) {
                        const auto interval = graph->interval(parameter, false);
                        replacement->setInterval(parameter, interval.first, interval.second);
                    }
                }
            }
            updatePlot(row, replacement);
        }
    }
    synchronize();
}
#include "moc_sessionplotsmodel.cpp"
