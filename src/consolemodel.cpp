/*************************************************************************************
 *  Copyright (C) 2007-2017 by Aleix Pol <aleixpol@kde.org>                          *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU General Public License                      *
 *  as published by the Free Software Foundation; either version 2                   *
 *  of the License, or (at your option) any later version.                           *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
 *************************************************************************************/

#include "consolemodel.h"
#include "contrastcolors_p.h"

#include <KLocalizedString>
#include <QFile>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;

ConsoleModel::ConsoleModel(QObject *parent)
    : QObject(parent)
{
}

bool ConsoleModel::addOperation(const QString &input)
{
    return addOperation(Analitza::Expression(input), input);
}

bool ConsoleModel::addOperation(const Analitza::Expression &e, const QString &input)
{
    Analitza::Expression res;

    a.setExpression(e);
    if (a.isCorrect()) {
        if (m_mode == ConsoleModel::Evaluation) {
            res = a.evaluate();
        } else {
            res = a.calculate();
        }
    }

    if (a.isCorrect()) {
        a.insertVariable(u"ans"_s, res);
        m_script += e; // Script won't have the errors
        Q_EMIT operationSuccessful(e, res);

        const auto result = res.toHtml();
        addMessage(QStringLiteral("<a title='%1' href='kalgebra:/query?id=copy&func=%2'><span "
                                  "class='exp'>%3</span></a><br />= <a title='kalgebra:%1' "
                                  "href='kalgebra:/query?id=copy&func=%4'><span "
                                  "class='result'>%5</span></a>")
                       .arg(i18n("Paste to Input"), e.toString(), e.toHtml(), res.toString(), result),
                   e,
                   res);
    } else {
        addMessage(i18n("<ul class='error'>Error: <b>%1</b><li>%2</li></ul>", input.toHtmlEscaped(), a.errors().join(u"</li>\n<li>"_s)), {}, {});
    }

    return a.isCorrect();
}

bool ConsoleModel::loadScript(const QUrl &path)
{
    Q_ASSERT(!path.isEmpty() && path.isLocalFile());

    // FIXME: We have expression-only script support
    bool correct = false;
    QFile file(path.toLocalFile());

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);

        a.importScript(&stream);
        correct = a.isCorrect();
    }

    if (correct)
        addMessage(i18n("Imported: %1", path.toDisplayString()), {}, {});
    else
        addMessage(i18n("<ul class='error'>Error: Could not load %1. <br /> %2</ul>", path.toDisplayString(), a.errors().join(u"<br/>"_s)), {}, {});

    return correct;
}

bool ConsoleModel::saveScript(const QUrl &savePath)
{
    Q_ASSERT(!savePath.isEmpty());

    QFile file(savePath.toLocalFile());
    bool correct = file.open(QIODevice::WriteOnly | QIODevice::Text);

    if (correct) {
        QTextStream out(&file);
        for (const Analitza::Expression &exp : std::as_const(m_script)) {
            out << exp.toString() << QLatin1Char('\n');
        }
    }

    return correct;
}

void ConsoleModel::setMode(ConsoleMode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        Q_EMIT modeChanged(mode);
    }
}

void ConsoleModel::setVariables(const QSharedPointer<Analitza::Variables> &vars)
{
    a.setVariables(vars);
}

void ConsoleModel::addMessage(const QString &msg, const Analitza::Expression &operation, const Analitza::Expression &result)
{
    m_htmlLog += msg.toUtf8();
    Q_EMIT updateView();
    Q_EMIT message(msg, operation, result);
}

bool ConsoleModel::saveLog(const QUrl &savePath) const
{
    Q_ASSERT(savePath.isLocalFile());
    // FIXME: We have to choose between txt and html
    QFile file(savePath.toLocalFile());
    bool correct = file.open(QIODevice::WriteOnly | QIODevice::Text);

    if (correct) {
        QTextStream out(&file);
        out << "<html>\n<head>" << css() << "</head>" << QLatin1Char('\n');
        out << "<body>" << QLatin1Char('\n');
        for (const QByteArray &entry : std::as_const(m_htmlLog)) {
            out << "<p>" << entry << "</p>" << QLatin1Char('\n');
        }
        out << "</body>\n</html>" << QLatin1Char('\n');
    }

    return correct;
}

void ConsoleModel::clear()
{
    m_script.clear();
    m_htmlLog.clear();
}

QByteArray ConsoleModel::css() const
{
    const QPalette palette = qGuiApp->palette();
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::Text);
    const QColor link = Analitza::readableColor(palette.color(QPalette::Link), base, text);
    const QColor variable = Analitza::readableColor(palette.color(QPalette::LinkVisited), base, text);
    // Tint the base lightly so the same token colors remain readable in every
    // row.
    auto tint = [&](const QColor &color) {
        return QColor::fromRgbF(base.redF() * 0.94 + color.redF() * 0.06,
                                base.greenF() * 0.94 + color.greenF() * 0.06,
                                base.blueF() * 0.94 + color.blueF() * 0.06)
            .name();
    };
    return QStringLiteral(
               "<style type=\"text/css\">"
               "html { background-color:%1; color:%2; }"
               ".error { border:1px solid %3; background-color:%4; padding:7px; }"
               ".last { border:1px solid %5; background-color:%6; padding:7px; }"
               ".normal:hover { background-color:%6; }"
               ".before { text-align:right; } .op, .sep { font-weight:bold; }"
               ".cont, .var, .string { color:%7; }"
               ".num, .sep, .keyword, .func, a { color:%5; }"
               ".exp { color:%2; } .result { padding-left:10%; }"
               ".options { font-size:small; text-align:right; }"
               "li { padding-left:12px; padding-bottom:4px; "
               "list-style-position:inside; }"
               "a:link, a:visited { text-decoration:none; }"
               "a:hover, a:active { text-decoration:underline; }"
               "p { font-size:%8px; } </style>")
        .arg(base.name(), text.name(), Analitza::readableColor(Qt::red, base, text).name(), tint(Qt::red), link.name(), tint(link), variable.name())
        .arg(QFontMetrics(QGuiApplication::font()).height())
        .toUtf8();
}

QString ConsoleModel::readContent(const QUrl &url)
{
    return QUrlQuery(url).queryItemValue(u"func"_s);
}

#include "moc_consolemodel.cpp"
