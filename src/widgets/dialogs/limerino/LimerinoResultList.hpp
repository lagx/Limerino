// SPDX-License-Identifier: MIT
// Reusable sortable/paginated result table for Limerino command surfaces.
// Shape: title row, table with two columns (Key / Value) by default, status
// line at the bottom, per-row "Copy" action, 25 rows per page.
// Consumers: batch 1 (/modlist), 2 (follows), 5 (history), 9 (roles), 10 (modlogs).

#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

namespace chatterino::limerino {

class LimerinoResultList : public QWidget
{
    Q_OBJECT

public:
    explicit LimerinoResultList(QWidget *parent = nullptr);

    void setTitleText(const QString &title);
    void setColumns(const QStringList &headers);
    void setRows(const QVector<QStringList> &rows);
    void setStatusText(const QString &status);
    void clear();

private:
    void applyPage();
    void copyCell(int row, int column);

    static constexpr int PAGE_SIZE = 25;

    QLabel *titleLabel_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *prevButton_ = nullptr;
    QPushButton *nextButton_ = nullptr;
    QLabel *pageLabel_ = nullptr;

    QStringList headers_;
    QVector<QStringList> rows_;
    int page_ = 0;
};

}  // namespace chatterino::limerino
