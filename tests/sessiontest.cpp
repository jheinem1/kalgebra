// SPDX-License-Identifier: GPL-2.0-or-later
#include "consolemodel.h"
#include "sessionplotsmodel.h"
#include <QGuiApplication>
#include <QPalette>
#include <QSignalSpy>
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
        QVERIFY(!variables->contains(QStringLiteral("f1")));
        QCOMPARE(calculate(variables, QStringLiteral("doubleIt(3)")), 6.);
        plots.removeRow(0);
        QVERIFY(!variables->contains(QStringLiteral("doubleIt")));
        QCOMPARE(calculate(variables, QStringLiteral("f0")), 42.);
    }
    void removingCalculatorFunctionsRemovesTheirGraphs()
    {
        ConsoleModel calculator;
        auto variables = calculator.variables();
        SessionPlotsModel twoD(variables), threeD(variables);
        add(twoD, QStringLiteral("x=13*y"), Analitza::Dim2D, variables);
        add(twoD, QStringLiteral("x->x^2"), Analitza::Dim2D, variables);
        add(threeD, QStringLiteral("(x,y)->x+y"), Analitza::Dim3D, variables);
        variables->remove(QStringLiteral("f0"));
        variables->remove(QStringLiteral("f2"));
        twoD.refresh();
        threeD.refresh();
        QCOMPARE(twoD.rowCount(), 1);
        QCOMPARE(twoD.index(0, 0).data().toString(), QStringLiteral("f1"));
        QCOMPARE(threeD.rowCount(), 0);
        QVERIFY(!variables->contains(QStringLiteral("f0")));
        QVERIFY(!variables->contains(QStringLiteral("f2")));
        QVERIFY(!variables->functionOverload(QStringLiteral("f0"), 1));
        QCOMPARE(calculate(variables, QStringLiteral("f1(3)")), 9.);
        twoD.refresh();
        QVERIFY(!variables->contains(QStringLiteral("f0")));
    }
    void removingGraphsRemovesCalculatorFunctions()
    {
        auto variables = QSharedPointer<Analitza::Variables>::create();
        SessionPlotsModel twoD(variables), threeD(variables);
        add(twoD, QStringLiteral("x=13*y"), Analitza::Dim2D, variables);
        add(twoD, QStringLiteral("x->x^2"), Analitza::Dim2D, variables);
        add(threeD, QStringLiteral("(x,y)->x+y"), Analitza::Dim3D, variables);
        QSignalSpy changes(&twoD, &SessionPlotsModel::functionsChanged);
        QVERIFY(twoD.removeRows(0, 2));
        QVERIFY(!changes.isEmpty());
        QVERIFY(!variables->contains(QStringLiteral("f0")));
        QVERIFY(!variables->contains(QStringLiteral("f1")));
        QVERIFY(!variables->functionOverload(QStringLiteral("f0"), 1));
        QVERIFY(variables->contains(QStringLiteral("f2")));
        threeD.clear();
        QVERIFY(!variables->contains(QStringLiteral("f2")));
    }
    void editsSynchronizeInBothGraphTabs()
    {
        ConsoleModel calculator;
        auto variables = calculator.variables();
        SessionPlotsModel twoD(variables), threeD(variables);
        add(twoD, QStringLiteral("x->x^2"), Analitza::Dim2D, variables);
        add(threeD, QStringLiteral("(x,y)->x+y"), Analitza::Dim3D, variables);
        QVERIFY(calculator.addOperation(QStringLiteral("f0:=x->x^3")));
        QVERIFY(calculator.addOperation(QStringLiteral("f1:=(x,y)->x*y")));
        twoD.refresh();
        threeD.refresh();
        QCOMPARE(twoD.index(0, 1).data().toString(), QStringLiteral("x->x^3"));
        QCOMPARE(threeD.index(0, 1).data().toString(), Analitza::Expression(QStringLiteral("(x,y)->x*y")).toString());
        QVERIFY(twoD.setData(twoD.index(0, 1), QStringLiteral("x->2*x")));
        QVERIFY(threeD.setData(threeD.index(0, 1), QStringLiteral("(x,y)->2*x+y")));
        QCOMPARE(calculate(variables, QStringLiteral("f0(3)")), 6.);
        QCOMPARE(calculate(variables, QStringLiteral("f1(3,4)")), 10.);
        QVERIFY(calculator.addOperation(QStringLiteral("f0:=(x,y,z,w)->x+y+z+w")));
        twoD.refresh();
        QCOMPARE(twoD.rowCount(), 0);
        QCOMPARE(calculate(variables, QStringLiteral("f0(1,2,3,4)")), 10.);
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
        QCOMPARE(calculate(variables, QStringLiteral("f0(2)")), 1.);
    }
    void implicitCalculatorEditsKeepShortCall()
    {
        ConsoleModel calculator;
        auto variables = calculator.variables();
        SessionPlotsModel plots(variables);
        add(plots, QStringLiteral("x=13*y"), Analitza::Dim2D, variables);
        for (int repeat = 0; repeat < 2; ++repeat) {
            QVERIFY(calculator.addOperation(QStringLiteral("f0:=(x,y)->x-26*y")));
            plots.refresh();
            QCOMPARE(plots.rowCount(), 1);
            QCOMPARE(calculate(variables, QStringLiteral("f0(52)")), 2.);
            QCOMPARE(calculate(variables, QStringLiteral("f0(52,2)")), 0.);
            QVERIFY(Analitza::Expression(plots.index(0, 1).data().toString()).isEquation());
        }
    }
    void implicitSingleArgument_data()
    {
        QTest::addColumn<QString>("equation");
        QTest::addColumn<double>("x");
        QTest::addColumn<double>("y");
        QTest::newRow("screenshot") << QStringLiteral("x=13*y") << 12. << 12./13.;
        QTest::newRow("parabola") << QStringLiteral("y=x^2") << 3. << 9.;
        QTest::newRow("both sides") << QStringLiteral("2*y+x=5*y-6") << 3. << 3.;
        QTest::newRow("variable coefficient") << QStringLiteral("x*y=1") << 2. << 0.5;
        QTest::newRow("division") << QStringLiteral("y/2=x+1") << 2. << 6.;
        QTest::newRow("negative") << QStringLiteral("-y=x") << 2. << -2.;
        QTest::newRow("trigonometric") << QStringLiteral("y=sin(x)") << 0. << 0.;
    }
    void implicitSingleArgument()
    {
        QFETCH(QString, equation);
        QFETCH(double, x);
        QFETCH(double, y);
        ConsoleModel calculator;
        auto variables = calculator.variables();
        SessionPlotsModel plots(variables);
        add(plots, equation, Analitza::Dim2D, variables);
        const QString call = QStringLiteral("f0(%1)").arg(x);
        QVERIFY(calculator.addOperation(call));
        QCOMPARE(variables->valueExpression(QStringLiteral("ans")).toReal().value(), y);
        QCOMPARE(calculate(variables, QStringLiteral("f0(%1,%2)").arg(x).arg(y, 0, 'g', 17)), 0.);
        QVERIFY(calculator.addOperation(QStringLiteral("g:=x->f0(x)+1")));
        QCOMPARE(calculate(variables, QStringLiteral("g(%1)").arg(x)), y+1);
    }
    void implicitEditsAndAmbiguousCurves()
    {
        ConsoleModel calculator;
        auto variables = calculator.variables();
        SessionPlotsModel plots(variables);
        QVERIFY(calculator.addOperation(QStringLiteral("a:=13")));
        add(plots, QStringLiteral("x=a*y"), Analitza::Dim2D, variables);
        QCOMPARE(calculate(variables, QStringLiteral("f0(13)")), 1.);
        QVERIFY(calculator.addOperation(QStringLiteral("a:=2")));
        plots.refresh();
        QCOMPARE(calculate(variables, QStringLiteral("f0(12)")), 6.);
        QVERIFY(plots.setData(plots.index(0, 1), QStringLiteral("x=3*y")));
        QCOMPARE(calculate(variables, QStringLiteral("f0(12)")), 4.);
        QVERIFY(plots.setData(plots.index(0, 0), QStringLiteral("line")));
        QVERIFY(!variables->contains(QStringLiteral("f0")));
        QCOMPARE(calculate(variables, QStringLiteral("line(12)")), 4.);
        plots.removeRow(0);
        QVERIFY(!variables->contains(QStringLiteral("line")));
        QVERIFY(!variables->functionOverload(QStringLiteral("line"), 1));
        add(plots, QStringLiteral("x^2+y^2=1"), Analitza::Dim2D, variables);
        QVERIFY(!calculator.addOperation(QStringLiteral("f0(0)")));
        QVERIFY(calculator.addOperation(QStringLiteral("f0(0,1)")));
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
    void rationalResultsAndFormatSwitch()
    {
        ConsoleModel calculator;
        QSignalSpy messages(&calculator, &ConsoleModel::message);
        auto shown = [&] { return qvariant_cast<Analitza::Expression>(messages.last().at(2)).toString(); };
        QVERIFY(calculator.addOperation(QStringLiteral("1/3+1/6")));
        QCOMPARE(shown(), QStringLiteral("1/2"));
        calculator.setResultFormat(ConsoleModel::Decimals);
        QVERIFY(calculator.htmlLog().last().contains("func=0.5"));
        calculator.setResultFormat(ConsoleModel::Fractions);
        QVERIFY(calculator.htmlLog().last().contains("func=1/2"));
        QVERIFY(calculator.addOperation(QStringLiteral("a:=1/3")));
        QCOMPARE(shown(), QStringLiteral("1/3"));
        QVERIFY(calculator.addOperation(QStringLiteral("ans+1/6")));
        QCOMPARE(shown(), QStringLiteral("1/2"));
        QVERIFY(calculator.addOperation(QStringLiteral("a+1/6")));
        QCOMPARE(shown(), QStringLiteral("1/2"));
        QVERIFY(calculator.addOperation(QStringLiteral("a:=1/2")));
        QCOMPARE(shown(), QStringLiteral("1/2"));
        QVERIFY(calculator.addOperation(QStringLiteral("a+1/6")));
        QCOMPARE(shown(), QStringLiteral("2/3"));
        QVERIFY(calculator.addOperation(QStringLiteral("0.1+0.2")));
        QCOMPARE(shown(), QStringLiteral("3/10"));
        QVERIFY(calculator.addOperation(QStringLiteral("(2/3)^(-2)")));
        QCOMPARE(shown(), QStringLiteral("9/4"));
        QVERIFY(calculator.addOperation(QStringLiteral("pi")));
        QCOMPARE(shown(), calculator.variables()->valueExpression(QStringLiteral("pi")).toString());
        QVERIFY(calculator.addOperation(QStringLiteral("sin(1)")));
        QVERIFY(!shown().contains(QLatin1Char('/')));
        calculator.setMode(ConsoleModel::Calculation);
        QVERIFY(calculator.addOperation(QStringLiteral("1/7")));
        QCOMPARE(shown(), QStringLiteral("1/7"));
        SessionPlotsModel plots(calculator.variables());
        add(plots, QStringLiteral("x=13*y"), Analitza::Dim2D, calculator.variables());
        QVERIFY(calculator.addOperation(QStringLiteral("f0(12)")));
        QCOMPARE(shown(), QStringLiteral("12/13"));
        calculator.setResultFormat(ConsoleModel::Decimals);
        QVERIFY(calculator.addOperation(QStringLiteral("f0(12)")));
        QVERIFY(!shown().contains(QLatin1Char('/')));
        calculator.clear();
        calculator.setResultFormat(ConsoleModel::Fractions);
        QVERIFY(calculator.htmlLog().isEmpty());
    }
};
QTEST_MAIN(SessionTest)
#include "sessiontest.moc"
