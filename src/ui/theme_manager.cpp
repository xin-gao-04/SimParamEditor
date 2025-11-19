#include "theme_manager.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

void ThemeManager::applyTheme(QApplication& app, ThemeVariant variant)
{
    const QString styleTemplate = loadTemplate();
    if (styleTemplate.isEmpty()) {
        qWarning() << "[ThemeManager] Failed to load style template.";
        return;
    }

    const QString resolved = applyTokens(styleTemplate, variant);
    app.setStyleSheet(resolved);
    m_current = variant;
}

void ThemeManager::toggleTheme(QApplication& app)
{
    const ThemeVariant next = (m_current == ThemeVariant::Light)
        ? ThemeVariant::Dark
        : ThemeVariant::Light;
    applyTheme(app, next);
}

QString ThemeManager::loadTemplate() const
{
    QFile resFile(":/styles/app.qss");
    if (resFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&resFile);
        const QString data = in.readAll();
        resFile.close();
        return data;
    }

    QFile localFile("resources/styles/app.qss");
    if (localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&localFile);
        const QString data = in.readAll();
        localFile.close();
        return data;
    }

    return {};
}

QString ThemeManager::applyTokens(const QString& styleTemplate, ThemeVariant variant) const
{
    QString css = styleTemplate;
    const auto tokens = tokensFor(variant);
    QStringList keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b){
        return a.length() > b.length();
    });

    for (const QString& key : keys) {
        css.replace(key, tokens.value(key), Qt::CaseSensitive);
    }
    return css;
}

QMap<QString, QString> ThemeManager::tokensFor(ThemeVariant variant) const
{
    if (variant == ThemeVariant::Dark) {
        return {
            { "@font-family-base", "\"Segoe UI\", \"Microsoft YaHei\", sans-serif" },
            { "@font-size-base", "14px" },
            { "@color-text-primary", "#E6EDF3" },
            { "@color-text-secondary", "#9EA7B3" },
            { "@color-bg-base", "#0D1117" },
            { "@color-bg-surface", "#161B22" },
            { "@color-border-light", "#1F242C" },
            { "@color-border-strong", "#30363D" },
            { "@color-primary", "#1F6FEB" },
            { "@color-primary-hover", "#388BFF" },
            { "@color-primary-active", "#1158C7" },
            { "@color-highlight", "rgba(31,111,235,0.16)" },
            { "@color-highlight-strong", "rgba(56,139,255,0.32)" },
            { "@color-tree-hover", "rgba(255,255,255,0.04)" },
            { "@color-tree-selected", "rgba(31,111,235,0.25)" },
            { "@color-card-shadow", "rgba(6,11,17,0.4)" },
            { "@color-error", "#FF6B6B" },
            { "@color-warning", "#E3B341" },
            { "@color-success", "#3FB950" },
            { "@radius-small", "4px" },
            { "@radius-medium", "8px" }
        };
    }

    // Light theme (default)
    return {
        { "@font-family-base", "\"Segoe UI\", \"Microsoft YaHei\", sans-serif" },
        { "@font-size-base", "14px" },
        { "@color-text-primary", "#333333" },
        { "@color-text-secondary", "#666666" },
        { "@color-bg-base", "#F5F7FA" },
        { "@color-bg-surface", "#FFFFFF" },
        { "@color-border-light", "#E0E0E0" },
        { "@color-border-strong", "#D9D9D9" },
        { "@color-primary", "#1890FF" },
        { "@color-primary-hover", "#40A9FF" },
        { "@color-primary-active", "#096DD9" },
        { "@color-highlight", "#E6F7FF" },
        { "@color-highlight-strong", "#BAE7FF" },
        { "@color-tree-hover", "#F5F5F5" },
        { "@color-tree-selected", "#E6F7FF" },
        { "@color-card-shadow", "rgba(15,23,42,0.12)" },
        { "@color-error", "#FF4D4F" },
        { "@color-warning", "#FAAD14" },
        { "@color-success", "#52C41A" },
        { "@radius-small", "4px" },
        { "@radius-medium", "8px" }
    };
}


