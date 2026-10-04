// SPDX-License-Identifier: GPL-2.0-or-later
#include "consolehtml.h"
#include "functionedit.h"
#include "kalgebra.h"
#include "sessionplotsmodel.h"
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTest>
#include <analitza/value.h>
#include <analitzagui/expressionedit.h>

class AppTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("kalgebra");
    }
    void sharedEditorsAndFunctions()
    {
        KAlgebra window;
        auto input = window.findChild<Analitza::ExpressionEdit *>(QStringLiteral("calculatorInput"));
        auto input3d = window.findChild<Analitza::ExpressionEdit *>(QStringLiteral("graph3dInput"));
        auto console = window.findChild<ConsoleHtml *>();
        auto twoD = window.findChild<SessionPlotsModel *>(QStringLiteral("graph2dFunctions"));
        auto threeD = window.findChild<SessionPlotsModel *>(QStringLiteral("graph3dFunctions"));
        auto editor = window.findChild<FunctionEdit *>();
        QVERIFY(input && input3d && console && twoD && threeD && editor);
        input->setText(QStringLiteral("square:=x->x**2"));
        QVERIFY(QMetaObject::invokeMethod(&window, "operate"));
        auto graphInput = editor->findChild<Analitza::ExpressionEdit *>(QStringLiteral("functionInput"));
        for (auto edit : {input3d, graphInput}) {
            auto completer = edit->findChild<QCompleter *>();
            completer->setCompletionPrefix(QStringLiteral("square"));
            QCOMPARE(completer->currentCompletion(), QStringLiteral("square"));
        }
        editor->setFunction(QStringLiteral("square(x)+1"));
        QVERIFY(QMetaObject::invokeMethod(&window, "new_func"));
        QCOMPARE(twoD->rowCount(), 1);
        input->setText(QStringLiteral("f0(4)"));
        QVERIFY(QMetaObject::invokeMethod(&window, "operate"));
        QCOMPARE(console->analitza()->variables()->valueExpression(QStringLiteral("ans")).toReal().value(), 17.);
        input3d->setText(QStringLiteral("(x,y)->f0(x)+y"));
        QVERIFY(QMetaObject::invokeMethod(&window, "new_func3d"));
        QCOMPARE(threeD->rowCount(), 1);
        input->setText(QStringLiteral("f1(4,2)"));
        QVERIFY(QMetaObject::invokeMethod(&window, "operate"));
        QCOMPARE(console->analitza()->variables()->valueExpression(QStringLiteral("ans")).toReal().value(), 19.);
        input->setText(QStringLiteral("square:=x->x**3"));
        QVERIFY(QMetaObject::invokeMethod(&window, "operate"));
        input->setText(QStringLiteral("f1(4,2)"));
        QVERIFY(QMetaObject::invokeMethod(&window, "operate"));
        QCOMPARE(console->analitza()->variables()->valueExpression(QStringLiteral("ans")).toReal().value(), 67.);
    }
    void darkConsoleAndThemeSwitch()
    {
        const auto original = qApp->palette();
        QPalette dark;
        dark.setColor(QPalette::Window, QColor(QStringLiteral("#292e32")));
        dark.setColor(QPalette::Base, QColor(QStringLiteral("#141618")));
        dark.setColor(QPalette::Text, QColor(QStringLiteral("#eff0f1")));
        dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#eff0f1")));
        dark.setColor(QPalette::Button, QColor(QStringLiteral("#292e32")));
        dark.setColor(QPalette::ButtonText, QColor(QStringLiteral("#eff0f1")));
        dark.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#232629")));
        dark.setColor(QPalette::Highlight, QColor(QStringLiteral("#69529b")));
        dark.setColor(QPalette::HighlightedText, QColor(Qt::white));
        dark.setColor(QPalette::Link, QColor(QStringLiteral("#1d99f3")));
        dark.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#9b59b6")));
        qApp->setPalette(dark);
        KAlgebra window;
        window.resize(1100, 720);
        window.show();
        auto console = window.findChild<ConsoleHtml *>();
        console->addOperation(Analitza::Expression(QStringLiteral("square:=x->x**2")), QStringLiteral("square:=x->x**2"));
        QSignalSpy loaded(console, &QWebEngineView::loadFinished);
        console->addOperation(Analitza::Expression(QStringLiteral("square(3)")), QStringLiteral("square(3)"));
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
        bool ready = false;
        QVariant value;
        console->page()->runJavaScript(QStringLiteral("getComputedStyle(document.documentElement).color"), [&](const QVariant &result) {
            value = result;
            ready = true;
        });
        QTRY_VERIFY(ready);
        QCOMPARE(value.toString(), QStringLiteral("rgb(239, 240, 241)"));
        const QString output = qEnvironmentVariable("KALGEBRA_TEST_SCREENSHOT_DIR");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            QTest::qWait(300);
            QVERIFY(window.grab().save(output + QStringLiteral("/kalgebra-breeze-dark.png")));
        }
        loaded.clear();
        qApp->setPalette(original);
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
        ready = false;
        console->page()->runJavaScript(QStringLiteral("getComputedStyle(document.documentElement).color"), [&](const QVariant &result) {
            value = result;
            ready = true;
        });
        QTRY_VERIFY(ready);
        QCOMPARE(value.toString(),
                 QStringLiteral("rgb(%1, %2, %3)")
                     .arg(original.color(QPalette::Text).red())
                     .arg(original.color(QPalette::Text).green())
                     .arg(original.color(QPalette::Text).blue()));
    }
};
QTEST_MAIN(AppTest)
#include "apptest.moc"
