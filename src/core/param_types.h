#ifndef PARAM_TYPES_H
#define PARAM_TYPES_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QMap>

enum class ParamType {
    UINT8, UINT16, UINT32,
    INT8, INT16, INT32,
    FLOAT, DOUBLE,
    CHAR, CHAR_ARRAY,
    ENUM, STRUCT
};

struct ParamMetadata {
    QString name;
    ParamType type;
    QString unit;
    QVariant defaultValue;
    QString description;
    QVector<ParamMetadata> children;
    
    // 新增：引用类型名称 (Type Registry Key)。
    // 如果设置了此字段，表示该节点是某个已注册类型的实例。
    // 此时 children/enumItems 应为空，具体结构由 TypeManager 中的定义决定。
    QString typeName;

    // enum
    QStringList enumItems;
    QVector<int> enumValues; // 可选：与 enumItems 对齐的数值，缺省时按顺序自增
    // array
    int arraySize = 0;
};

// 实例元数据：指向某个结构类型路径，并为其每个叶子字段提供值
struct InstanceMetadata {
    QString name;                  // 实例名称，如 Radar1
    QString typePath;              // 指向模板结构的路径，如 /PayloadConfig/SensorModule
    
    // 扁平化存储所有叶子节点的值
    // Key: 相对路径 (e.g. "Temperature", "GPS/Latitude")
    // Value: 具体值
    QMap<QString, QVariant> values;
    
    // 已移除递归 children，改为 Diff 模式
};

inline QString paramTypeToString(ParamType t)
{
    switch (t) {
    case ParamType::UINT8: return "uint8";    case ParamType::UINT16: return "uint16";  case ParamType::UINT32: return "uint32";
    case ParamType::INT8:  return "int8";     case ParamType::INT16:  return "int16";   case ParamType::INT32:  return "int32";
    case ParamType::FLOAT: return "float";    case ParamType::DOUBLE: return "double";
    case ParamType::CHAR:  return "char";     case ParamType::CHAR_ARRAY: return "char[]";
    case ParamType::ENUM:  return "enum";     case ParamType::STRUCT: return "struct";
    }
    return "unknown";
}

inline ParamType stringToParamType(const QString& s)
{
    if (s == "uint8") return ParamType::UINT8;    if (s == "uint16") return ParamType::UINT16;  if (s == "uint32") return ParamType::UINT32;
    if (s == "int8")  return ParamType::INT8;     if (s == "int16")  return ParamType::INT16;   if (s == "int32")  return ParamType::INT32;
    if (s == "float") return ParamType::FLOAT;    if (s == "double") return ParamType::DOUBLE;
    if (s == "char")  return ParamType::CHAR;     if (s == "char[]") return ParamType::CHAR_ARRAY;
    if (s == "enum")  return ParamType::ENUM;     return ParamType::STRUCT;
}

inline bool canHaveChildren(ParamType t)
{
    return t == ParamType::STRUCT;
}

inline bool isNumericType(ParamType t)
{
    switch (t) {
    case ParamType::UINT8: case ParamType::UINT16: case ParamType::UINT32:
    case ParamType::INT8:  case ParamType::INT16:  case ParamType::INT32:
    case ParamType::FLOAT: case ParamType::DOUBLE:
        return true;
    default: return false;
    }
}

inline QPair<qint64, qint64> integerRange(ParamType t)
{
    switch (t) {
    case ParamType::UINT8:  return qMakePair<qint64,qint64>(0, 255);
    case ParamType::UINT16: return qMakePair<qint64,qint64>(0, 65535);
    case ParamType::UINT32: return qMakePair<qint64,qint64>(0, 4294967295LL);
    case ParamType::INT8:   return qMakePair<qint64,qint64>(-128, 127);
    case ParamType::INT16:  return qMakePair<qint64,qint64>(-32768, 32767);
    case ParamType::INT32:  return qMakePair<qint64,qint64>(-2147483648LL, 2147483647LL);
    default: return QPair<qint64,qint64>();
    }
}

#endif // PARAM_TYPES_H


