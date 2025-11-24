#include "ui/main_window/main_widget.h"

#include "core/validation.h"

#include <QLabel>
#include <QMessageBox>

void MainWidget::updateValidationStatus()
{
    auto report = validateProject(rootParam);
    int errors = 0;
    int warns = 0;
    for (const auto& i : report.issues) {
        if (i.level == ValidationIssue::Error) ++errors;
        else ++warns;
    }
    statusLabel->setText(QString::fromUtf8(u8"\u6821\u9A8C\u72B6\u6001: ") +
                         (errors == 0 ? QString::fromUtf8(u8"\u901A\u8FC7") : QString::fromUtf8(u8"\u9519\u8BEF ") + QString::number(errors)) +
                         QString::fromUtf8(u8"  \u8B66\u544A: ") + QString::number(warns));
}

void MainWidget::showValidationReportDialog()
{
    auto report = validateProject(rootParam);
    QString text;
    for (const auto& i : report.issues) {
        text += (i.level == ValidationIssue::Error ? QString::fromUtf8(u8"[\u9519\u8BEF] ") : QString::fromUtf8(u8"[\u8B66\u544A] ")) + i.path + " - " + i.message + "\n";
    }
    if (text.isEmpty()) text = QString::fromUtf8(u8"\u65E0\u95EE\u9898");
    QMessageBox::information(this, QString::fromUtf8(u8"\u6821\u9A8C\u62A5\u544A"), text);
}

void MainWidget::onShowValidationClicked()
{
    showValidationReportDialog();
}
