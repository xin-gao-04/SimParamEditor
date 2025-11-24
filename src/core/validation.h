#ifndef VALIDATION_H
#define VALIDATION_H

#include <QVector>
#include <QString>
#include "core/param_types.h"

struct ValidationIssue {
    enum Level { Error, Warning } level;
    QString path;
    QString message;
};

struct ValidationReport {
    QVector<ValidationIssue> issues;
    bool hasError() const {
        for (const auto& i : issues) if (i.level == ValidationIssue::Error) return true;
        return false;
    }
};

ValidationReport validateProject(const ParamMetadata& root);

#endif // VALIDATION_H

