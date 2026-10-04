// SPDX-License-Identifier: GPL-2.0-or-later
#include "exactrational.h"
#include <analitza/apply.h>
#include <analitza/container.h>
#include <analitza/value.h>
#include <analitza/variable.h>
#include <cmath>
#include <limits>
#include <optional>

namespace
{
struct Rational {
    qint64 numerator;
    qint64 denominator;
};
using Result = std::optional<Rational>;
__extension__ using Wide = __int128;

Result normalized(Wide numerator, Wide denominator)
{
    if (!denominator)
        return {};
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    auto a = numerator < 0 ? -numerator : numerator;
    auto b = denominator;
    while (b) {
        const auto remainder = a % b;
        a = b;
        b = remainder;
    }
    numerator /= a;
    denominator /= a;
    constexpr auto maximum = std::numeric_limits<qint64>::max();
    if (numerator > maximum || numerator < -maximum || denominator > maximum)
        return {};
    return Rational{qint64(numerator), qint64(denominator)};
}

Result number(const Analitza::Cn *value, bool literal)
{
    if (value->isBoolean() || value->format() == Analitza::Cn::Complex)
        return {};
    const double real = value->value();
    if (!std::isfinite(real) || std::abs(real) > 9007199254740991.)
        return {};
    if (std::floor(real) == real)
        return Rational{qint64(real), 1};
    if (!literal)
        return {};
    // Only expression literals are converted, never approximate constants or
    // transcendental results. Their canonical decimal spelling is a rational.
    QString text = value->toString();
    if (text.toDouble() != real)
        return {};
    const int exponentIndex = text.indexOf(QLatin1Char('e'), 0, Qt::CaseInsensitive);
    int exponent = 0;
    if (exponentIndex >= 0) {
        bool ok;
        exponent = text.mid(exponentIndex + 1).toInt(&ok);
        if (!ok || std::abs(exponent) > 18)
            return {};
        text.truncate(exponentIndex);
    }
    const int point = text.indexOf(QLatin1Char('.'));
    int places = point < 0 ? 0 : text.size() - point - 1;
    text.remove(QLatin1Char('.'));
    bool ok;
    const qint64 digits = text.toLongLong(&ok);
    if (!ok || std::abs(places - exponent) > 18)
        return {};
    Wide numerator = digits, denominator = 1;
    for (int i = 0; i < places - exponent; ++i)
        denominator *= 10;
    for (int i = 0; i < exponent - places; ++i)
        numerator *= 10;
    return normalized(numerator, denominator);
}

class Evaluator
{
public:
    Evaluator(const QSharedPointer<Analitza::Variables> &variables, const ExactValues &values)
        : m_variables(variables), m_values(values)
    {
    }
    Result evaluate(const Analitza::Object *object, const QHash<QString, Rational> &scope = {}, int depth = 0, bool literal = true)
    {
        if (!object || depth > 64)
            return {};
        if (object->isContainer()) {
            const auto container = static_cast<const Analitza::Container *>(object);
            if (container->containerType() == Analitza::Container::math && container->m_params.size() == 1)
                return evaluate(container->m_params.first(), scope, depth + 1, literal);
            return {};
        }
        if (object->type() == Analitza::Object::value)
            return number(static_cast<const Analitza::Cn *>(object), literal);
        if (object->type() == Analitza::Object::variable) {
            const QString name = static_cast<const Analitza::Ci *>(object)->name();
            if (scope.contains(name))
                return scope.value(name);
            if (!m_variables->contains(name))
                return {};
            const auto definition = m_variables->valueExpression(name);
            const auto stored = m_values.constFind(name);
            if (stored != m_values.cend() && definition.isReal() && stored->first == definition.toReal().value())
                return evaluate(stored->second.tree(), {}, depth + 1);
            return evaluate(definition.tree(), {}, depth + 1, false);
        }
        if (!object->isApply())
            return {};
        const auto apply = static_cast<const Analitza::Apply *>(object);
        const auto op = apply->firstOperator().operatorType();
        if (op == Analitza::Operator::function) {
            const Analitza::Object *callee = apply->at(0);
            if (callee->type() == Analitza::Object::variable) {
                const QString name = static_cast<const Analitza::Ci *>(callee)->name();
                if (scope.contains(name) || !m_variables->contains(name))
                    return {};
                callee = m_variables->functionOverload(name, apply->countValues() - 1);
                if (!callee)
                    callee = m_variables->value(name);
            }
            if (!callee->isContainer())
                return {};
            const auto function = static_cast<const Analitza::Container *>(callee);
            const auto parameters = function->bvarStrings();
            if (function->containerType() != Analitza::Container::lambda || parameters.size() != apply->countValues() - 1)
                return {};
            QHash<QString, Rational> arguments;
            for (int i = 0; i < parameters.size(); ++i) {
                const auto argument = evaluate(apply->at(i + 1), scope, depth + 1, literal);
                if (!argument)
                    return {};
                arguments.insert(parameters.at(i), *argument);
            }
            return evaluate(function->m_params.last(), arguments, depth + 1);
        }
        if (op != Analitza::Operator::plus && op != Analitza::Operator::minus && op != Analitza::Operator::times && op != Analitza::Operator::divide
            && op != Analitza::Operator::power)
            return {};
        auto result = evaluate(apply->at(0), scope, depth + 1, literal);
        if (result && apply->isUnary() && op == Analitza::Operator::minus)
            result->numerator = -result->numerator;
        for (int i = 1; result && i < apply->countValues(); ++i) {
            const auto next = evaluate(apply->at(i), scope, depth + 1, literal);
            if (!next)
                return {};
            const Wide a = result->numerator, b = result->denominator, c = next->numerator, d = next->denominator;
            if (op == Analitza::Operator::plus || op == Analitza::Operator::minus)
                result = normalized(a*d + (op == Analitza::Operator::plus ? c*b : -c*b), b*d);
            else if (op == Analitza::Operator::times)
                result = normalized(a*c, b*d);
            else if (op == Analitza::Operator::divide)
                result = normalized(a*d, b*c);
            else {
                if (next->denominator != 1 || std::abs(next->numerator) > 64)
                    return {};
                auto base = next->numerator < 0 ? normalized(b, a) : result;
                if (!base)
                    return {};
                result = Rational{1, 1};
                for (qint64 power = 0; result && power < std::abs(next->numerator); ++power)
                    result = normalized(Wide(result->numerator)*base->numerator, Wide(result->denominator)*base->denominator);
            }
        }
        return result;
    }
private:
    QSharedPointer<Analitza::Variables> m_variables;
    const ExactValues &m_values;
};
}

Analitza::Expression exactRational(const Analitza::Expression &expression, const QSharedPointer<Analitza::Variables> &variables, const ExactValues &values)
{
    const auto source = expression.isDeclaration() ? expression.declarationValue() : expression;
    const auto value = Evaluator(variables, values).evaluate(source.tree());
    if (!value)
        return {};
    // Analitza's numeric literals use doubles. Both integers must be exactly
    // representable when the fraction is converted back into an expression.
    if (std::abs(value->numerator) > 9007199254740991LL || value->denominator > 9007199254740991LL)
        return {};
    return Analitza::Expression(value->denominator == 1 ? QString::number(value->numerator)
                                                       : QStringLiteral("%1/%2").arg(value->numerator).arg(value->denominator));
}
