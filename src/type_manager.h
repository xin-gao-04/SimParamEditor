#ifndef TYPE_MANAGER_H
#define TYPE_MANAGER_H

#include "param_types.h"
#include <QMap>
#include <QString>
#include <QObject>

/**
 * @brief 类型管理器 (单例)
 * 负责管理所有用户定义的命名类型 (Structs, Enums)。
 * 实现了类型定义的集中存储，解决了类型引用和复用的问题。
 */
class TypeManager : public QObject {
    Q_OBJECT
public:
    static TypeManager& instance();

    /**
     * @brief 注册一个新类型
     * @param name 类型名称 (全局唯一)
     * @param meta 类型元数据
     * @return true 注册成功, false 如果名称已存在
     */
    bool registerType(const QString& name, const ParamMetadata& meta);

    /**
     * @brief 更新已存在的类型定义
     * @param name 类型名称
     * @param meta 新的元数据
     * @return true 更新成功, false 如果类型不存在
     */
    bool updateType(const QString& name, const ParamMetadata& meta);

    /**
     * @brief 获取类型定义
     * @param name 类型名称
     * @return 指向元数据的指针，如果不存在返回 nullptr
     */
    const ParamMetadata* getType(const QString& name) const;

    /**
     * @brief 检查类型是否存在
     */
    bool hasType(const QString& name) const;

    /**
     * @brief 获取所有注册的类型名称
     */
    QStringList getAllTypeNames() const;

    /**
     * @brief 获取特定种类的类型名称列表
     * @param type 过滤类型 (例如 ParamType::STRUCT)
     */
    QStringList getTypeNames(ParamType type) const;

    /**
     * @brief 删除类型
     * @param name 类型名称
     * @return true 删除成功
     */
    bool removeType(const QString& name);

    /**
     * @brief 清空所有类型
     */
    void clear();

signals:
    void typeRegistered(const QString& name);
    void typeUpdated(const QString& name);
    void typeRemoved(const QString& name);

private:
    TypeManager() {}
    QMap<QString, ParamMetadata> m_types;
};

#endif // TYPE_MANAGER_H

