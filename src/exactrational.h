// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXACTRATIONAL_H
#define EXACTRATIONAL_H
#include <QHash>
#include <analitza/expression.h>
#include <analitza/variables.h>

// Store the numeric definition alongside its exact result to detect redefinitions.
using ExactValues = QHash<QString, QPair<double, Analitza::Expression>>;
Analitza::Expression exactRational(const Analitza::Expression &expression, const QSharedPointer<Analitza::Variables> &variables, const ExactValues &values);
#endif
