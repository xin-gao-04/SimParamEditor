#ifndef CPP_GENERATOR_H
#define CPP_GENERATOR_H

#include <QString>
#include <QTextStream>
#include "../param_types.h"

// C++代码生成器
class CppGenerator {
public:
    // 生成所有结构体代码文件
    bool generate(const ParamMetadata& root, const QString& outDir);

    // 生成实例对象（聚合初始化/变量定义）
    bool generateInstances(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);

    // 生成实例对象对应的Json序列化相关代码
    bool generateInstancesJson(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);

private:
    // 写单个结构体（.h/.cpp）
    bool generateStruct(const ParamMetadata& node, const QString& outDir);

    // 写实例变量和对应头文件包含
    bool writeInstancesUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);

    // 渲染C++聚合初始化（如 {1, 2, {...}} ）
    QString renderAggregateInit(const ParamMetadata& typeNode, const InstanceMetadata& inst, const QString& prefix = "");

    // 写Json相关代码
    bool writeInstancesJsonUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir);

    // 渲染to_json逻辑
    void renderToJson(QTextStream& out, const ParamMetadata& typeNode, const QString& varName, const QString& jsonVar, int indent);

    // 渲染from_json逻辑
    void renderFromJson(QTextStream& out, const ParamMetadata& typeNode, const QString& varName, const QString& jsonVar, int indent);

    // C++类型映射
    static QString cppTypeFor(ParamType t);

    // 名称安全化
    static QString sanitize(const QString& name);

    // 驼峰小写
    static QString toLowerCamel(const QString& s);
};

#endif // CPP_GENERATOR_H


