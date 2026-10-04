// SPDX-License-Identifier: GPL-2.0-or-later
#include "sessionplotsmodel.h"

#include <QScopedValueRollback>
#include <analitza/analitzautils.h>
#include <analitza/analyzer.h>
#include <analitza/apply.h>
#include <analitza/variables.h>
#include <analitzaplot/functiongraph.h>
#include <analitzaplot/plotsfactory.h>

namespace
{
// Split a residual into coefficient*y + constant without sampling. This also
// works when the coefficient and constant depend on x or shared variables.
struct AffineY {
    QString coefficient = QStringLiteral("0");
    QString constant = QStringLiteral("0");
    bool valid = true;
};

QString combine(const QString &left, const QString &op, const QString &right)
{
    return QStringLiteral("(%1)%2(%3)").arg(left, op, right);
}

AffineY splitY(const Analitza::Object *object)
{
    if (!AnalitzaUtils::dependencies(object, {}).contains(QStringLiteral("y")))
        return {QStringLiteral("0"), object->toString(), true};
    if (object->type() == Analitza::Object::variable)
        return {QStringLiteral("1"), QStringLiteral("0"), true};
    if (!object->isApply())
        return {{}, {}, false};
    const auto apply = static_cast<const Analitza::Apply *>(object);
    const auto op = apply->firstOperator().operatorType();
    if (op != Analitza::Operator::plus && op != Analitza::Operator::minus && op != Analitza::Operator::times && op != Analitza::Operator::divide)
        return {{}, {}, false};
    AffineY result = splitY(apply->at(0));
    if (op == Analitza::Operator::minus && apply->isUnary()) {
        result.coefficient = combine(QStringLiteral("0"), QStringLiteral("-"), result.coefficient);
        result.constant = combine(QStringLiteral("0"), QStringLiteral("-"), result.constant);
    }
    for (int i = 1; result.valid && i < apply->countValues(); ++i) {
        const auto next = splitY(apply->at(i));
        if (!next.valid)
            return next;
        if (op == Analitza::Operator::times) {
            if (result.coefficient != QLatin1String("0") && next.coefficient != QLatin1String("0"))
                return {{}, {}, false};
            result.coefficient = result.coefficient == QLatin1String("0") ? combine(result.constant, QStringLiteral("*"), next.coefficient)
                                                                         : combine(result.coefficient, QStringLiteral("*"), next.constant);
            result.constant = combine(result.constant, QStringLiteral("*"), next.constant);
        } else if (op == Analitza::Operator::divide) {
            if (next.coefficient != QLatin1String("0"))
                return {{}, {}, false};
            result.coefficient = combine(result.coefficient, QStringLiteral("/"), next.constant);
            result.constant = combine(result.constant, QStringLiteral("/"), next.constant);
        } else {
            const QString symbol = op == Analitza::Operator::plus ? QStringLiteral("+") : QStringLiteral("-");
            result.coefficient = combine(result.coefficient, symbol, next.coefficient);
            result.constant = combine(result.constant, symbol, next.constant);
        }
    }
    return result;
}

Analitza::Expression ordinateFunction(const Analitza::Expression &residual)
{
    if (residual.bvarList() != QStringList{QStringLiteral("x"), QStringLiteral("y")})
        return {};
    const auto parts = splitY(residual.lambdaBody().tree());
    if (!parts.valid)
        return {};
    Analitza::Analyzer symbolic;
    symbolic.setExpression(Analitza::Expression(parts.coefficient));
    symbolic.simplify();
    if (!symbolic.isCorrect() || symbolic.expression().tree()->isZero())
        return {};
    const auto function = Analitza::Expression(QStringLiteral("x->-(%1)/(%2)").arg(parts.constant, parts.coefficient));
    symbolic.setExpression(function);
    symbolic.simplify();
    return symbolic.isCorrect() ? symbolic.expression() : Analitza::Expression();
}
}

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
    QHash<QString, Analitza::PlotItem *> items;
    for (int row = 0; row < rowCount(); ++row) {
        auto item = index(row, 0).data(PlotRole).value<Analitza::PlotItem *>();
        current.insert(item->name(), item->expression());
        items.insert(item->name(), item);
    }
    bool changed = false;
    // A graph and its named calculator definition have the same lifetime.
    // This also removes the old name after a graph is renamed.
    for (auto it = m_published.cbegin(); it != m_published.cend(); ++it) {
        if (!current.contains(it.key())) {
            changed |= m_variables->remove(it.key()) > 0;
        }
    }
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        // Cosmetic changes must not overwrite a function redefined in the
        // calculator.
        if (!m_published.contains(it.key()) || m_published.value(it.key()).toString() != it.value().toString()) {
            m_variables->modify(it.key(), it.value());
            changed = true;
        }
        const auto item = items.value(it.key());
        if (item->spaceDimension() == Analitza::Dim2D && Analitza::Expression(item->display()).isEquation()
            && m_variables->contains(it.key()) && m_variables->valueExpression(it.key()).toString() == it.value().toString()
            && !m_variables->functionOverload(it.key(), 1)) {
            const auto ordinate = ordinateFunction(it.value());
            if (ordinate.isLambda() && ordinate.isCorrect()) {
                m_variables->setFunctionOverload(it.key(), ordinate);
                changed = true;
            }
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
        for (int row = rowCount() - 1; row >= 0; --row) {
            auto item = index(row, 0).data(PlotRole).value<Analitza::PlotItem *>();
            const QString name = item->name();
            if (!m_variables->contains(name)) {
                // The calculator removed this function. Do not republish it
                // from a stale graph during synchronization.
                m_published.remove(name);
                removeRow(row);
                continue;
            }
            Analitza::Expression source(item->display());
            const auto definition = m_variables->valueExpression(name);
            const bool definitionChanged = m_published.contains(name) && definition.toString() != m_published.value(name).toString();
            if (definitionChanged)
                source = definition;
            auto builder = Analitza::PlotsFactory::self()->requestPlot(source, item->spaceDimension(), m_variables);
            if (!builder.canDraw()) {
                if (definitionChanged) {
                    // Keep the new calculator value, even if it no longer
                    // has the shape required by this graph tab.
                    m_published.remove(name);
                    removeRow(row);
                }
                continue;
            }
            auto replacement = builder.create(item->color(), item->name());
            if (definitionChanged && source.isLambda() && source.bvarList() == QStringList{QStringLiteral("x"), QStringLiteral("y")}
                && item->spaceDimension() == Analitza::Dim2D && Analitza::Expression(item->display()).isEquation()) {
                // Calculator edits supply the residual lambda. Retain its
                // equation form so the one-argument ordinate is rebuilt too.
                replacement->setDisplay(QStringLiteral("(%1)=0").arg(source.lambdaBody().toString()));
            }
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
