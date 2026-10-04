// SPDX-License-Identifier: GPL-2.0-or-later
#include "consolemodel.h"
#include "sessionplotsmodel.h"
#include <QGuiApplication>
#include <QPalette>
#include <QTest>
#include <analitza/analyzer.h>
#include <analitza/value.h>
#include <analitzaplot/functiongraph.h>
#include <analitzaplot/planecurve.h>
#include <analitzaplot/plotsfactory.h>

class SessionTest : public QObject
{
    Q_OBJECT
    static void add(SessionPlotsModel &model, const QString &source, Analitza::Dimension dimension, const QSharedPointer<Analitza::Variables> &variables)
    {
        auto builder = Analitza::PlotsFactory::self()->requestPlot(Analitza::Expression(source), dimension, variables);
        QVERIFY2(builder.canDraw(), qPrintable(builder.errors().join(QStringLiteral(", "))));
        model.addPlot(builder.create(Qt::green, model.nextFunctionName()));
    }
    static double calculate(const QSharedPointer<Analitza::Variables> &variables, const QString &source)
    {
        Analitza::Analyzer analyzer(variables);
        analyzer.setExpression(Analitza::Expression(source));
        const auto result = analyzer.calculate();
        if (!analyzer.isCorrect() || !result.isReal()) {
            QTest::qFail(qPrintable(analyzer.errors().join(QStringLiteral(", "))), __FILE__, __LINE__);
            return 0;
        }
        return result.toReal().value();
    }
private Q_SLOTS:
    void graphFunctionsAreShared()
    {
        ConsoleModel calculator;
        auto variables = calculator.variables();
        SessionPlotsModel twoD(variables), threeD(variables);
        add(twoD, QStringLiteral("x->x**2"), Analitza::Dim2D, variables);
        QCOMPARE(calculate(variables, QStringLiteral("f0(3)")), 9.);
        add(threeD, QStringLiteral("(x,y)->f0(x)+y"), Analitza::Dim3D, variables);
        QCOMPARE(calculate(variables, QStringLiteral("f1(3,4)")), 13.);
        QVERIFY(calculator.addOperation(QStringLiteral("square:=x->x**2")));
        add(twoD, QStringLiteral("square(x)+1"), Analitza::Dim2D, variables);
        QCOMPARE(calculate(variables, QStringLiteral("f2(4)")), 17.);
        QVERIFY(twoD.setData(twoD.index(0, 1), QStringLiteral("x->x**3")));
        QCOMPARE(calculate(variables, QStringLiteral("f0(3)")), 27.);
        threeD.refresh();
        QCOMPARE(calculate(variables, QStringLiteral("f1(3,4)")), 31.);
    }
    void renameRemoveAndCollisions()
    {
        auto variables = QSharedPointer<Analitza::Variables>::create();
        variables->modify(QStringLiteral("f0"), Analitza::Expression(QStringLiteral("42")));
        SessionPlotsModel plots(variables);
        add(plots, QStringLiteral("x->2*x"), Analitza::Dim2D, variables);
        QCOMPARE(plots.index(0, 0).data().toString(), QStringLiteral("f1"));
        QVERIFY(!plots.setData(plots.index(0, 0), QStringLiteral("f0")));
        QVERIFY(!plots.setData(plots.index(0, 0), QStringLiteral("not a name")));
        QVERIFY(plots.setData(plots.index(0, 0), QStringLiteral("doubleIt")));
        QCOMPARE(calculate(variables, QStringLiteral("f1(3)")), 6.);
        QCOMPARE(calculate(variables, QStringLiteral("doubleIt(3)")), 6.);
        plots.removeRow(0);
        QCOMPARE(calculate(variables, QStringLiteral("doubleIt(3)")), 6.);
        QCOMPARE(calculate(variables, QStringLiteral("f0")), 42.);
    }
    void calculatorRedefinitionSurvivesCosmeticChanges()
    {
        auto variables = QSharedPointer<Analitza::Variables>::create();
        SessionPlotsModel plots(variables);
        add(plots, QStringLiteral("x->x**2"), Analitza::Dim2D, variables);
        variables->modify(QStringLiteral("f0"), Analitza::Expression(QStringLiteral("x->x**3")));
        QVERIFY(plots.setData(plots.index(0, 0), Qt::Unchecked, Qt::CheckStateRole));
        QCOMPARE(calculate(variables, QStringLiteral("f0(2)")), 8.);
        plots.refresh();
        QCOMPARE(plots.index(0, 0).data(Analitza::PlotsModel::PlotRole).value<Analitza::PlotItem *>()->expression().toString(), QStringLiteral("x->x^3"));
        QCOMPARE(calculate(variables, QStringLiteral("f0(2)")), 8.);
    }
    void changedVariablesRebuildCurve()
    {
        auto variables = QSharedPointer<Analitza::Variables>::create();
        variables->modify(QStringLiteral("a"), 2.);
        SessionPlotsModel plots(variables);
        add(plots, QStringLiteral("x->a*x"), Analitza::Dim2D, variables);
        variables->modify(QStringLiteral("a"), 3.);
        plots.refresh();
        auto curve = dynamic_cast<Analitza::PlaneCurve *>(plots.index(0, 0).data(Analitza::PlotsModel::PlotRole).value<Analitza::PlotItem *>());
        QVERIFY(curve);
        QCOMPARE(calculate(QSharedPointer<Analitza::Variables>::create(*curve->variables()), QStringLiteral("a")), 3.);
        QCOMPARE(calculate(variables, QStringLiteral("f0(2)")), 6.);
    }
    void implicitEquationKeepsAllArguments()
    {
        auto variables = QSharedPointer<Analitza::Variables>::create();
        SessionPlotsModel plots(variables);
        add(plots, QStringLiteral("x=2*y"), Analitza::Dim2D, variables);
        QCOMPARE(variables->valueExpression(QStringLiteral("f0")).bvarList().size(), 2);
        QCOMPARE(calculate(variables, QStringLiteral("f0(2,1)")), 0.);
    }
    void cssFollowsPalette()
    {
        const auto original = qGuiApp->palette();
        QPalette dark;
        dark.setColor(QPalette::Base, QColor(QStringLiteral("#141618")));
        dark.setColor(QPalette::Text, QColor(QStringLiteral("#eff0f1")));
        dark.setColor(QPalette::Link, QColor(QStringLiteral("#1d99f3")));
        dark.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#9b59b6")));
        qGuiApp->setPalette(dark);
        ConsoleModel model;
        const auto darkCss = model.css();
        QVERIFY(darkCss.contains("color:#eff0f1"));
        QVERIFY(!darkCss.contains("#000000"));
        qGuiApp->setPalette(original);
        QVERIFY(model.css() != darkCss);
    }
};
QTEST_MAIN(SessionTest)
#include "sessiontest.moc"
