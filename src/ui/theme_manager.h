#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QApplication>
#include <QMap>
#include <QString>

class ThemeManager
{
public:
    enum class ThemeVariant {
        Light,
        Dark
    };

    static ThemeManager& instance();

    void applyTheme(QApplication& app, ThemeVariant variant);
    void toggleTheme(QApplication& app);
    ThemeVariant currentTheme() const { return m_current; }

private:
    ThemeManager() = default;

    QString loadTemplate() const;
    QString applyTokens(const QString& styleTemplate, ThemeVariant variant) const;
    QMap<QString, QString> tokensFor(ThemeVariant variant) const;

    ThemeVariant m_current = ThemeVariant::Light;
};

#endif // THEME_MANAGER_H


