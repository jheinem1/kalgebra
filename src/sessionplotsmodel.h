// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SESSIONPLOTSMODEL_H
#define SESSIONPLOTSMODEL_H

#include <analitza/expression.h>
#include <analitzaplot/plotsmodel.h>

// Publish named plots in the same symbol table as the calculator.
class SessionPlotsModel : public Analitza::PlotsModel
{
    Q_OBJECT
public:
    SessionPlotsModel(const QSharedPointer<Analitza::Variables> &variables, QObject *parent = nullptr);
    ~SessionPlotsModel() override;
    QString nextFunctionName() const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void refresh();

Q_SIGNALS:
    void functionsChanged();

private:
    void synchronize();
    QSharedPointer<Analitza::Variables> m_variables;
    QHash<QString, Analitza::Expression> m_published;
    bool m_synchronizing = false;
};
#endif
