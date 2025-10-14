#ifndef CPP_GENERATOR_H
#define CPP_GENERATOR_H

#include <QString>
#include <QTextStream>
#include "../param_types.h"

class CppGenerator {
public:
    bool generate(const ParamMetadata& root, const QString& outDir);
    bool generateInstances(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);
    bool generateInstancesJson(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);

private:
    bool generateStruct(const ParamMetadata& node, const QString& outDir);
    bool writeInstancesUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);
    QString renderAggregateInit(const ParamMetadata& typeNode, const InstanceMetadata& inst);
    bool writeInstancesJsonUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);
    void renderToJson(QTextStream& out, const ParamMetadata& typeNode, const QString& varName, const QString& jsonVar, int indent);
    void renderFromJson(QTextStream& out, const ParamMetadata& typeNode, const QString& varName, const QString& jsonVar, int indent);
    static QString cppTypeFor(ParamType t);
    static QString sanitize(const QString& name);
    static QString toLowerCamel(const QString& s);
};

#endif // CPP_GENERATOR_H


