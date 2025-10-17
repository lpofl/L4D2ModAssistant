
#include "app/ui/MainWindow.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QWidget>
#include "core/repo/RepositoryService.h"
#include "core/config/Settings.h"
#include "core/db/Db.h"
#include "core/db/Migrations.h"
#include "core/log/Log.h"

/**
 * @file MainWindow.cpp
 * @brief 主窗口实现：初始化数据库与 UI，展示可见 Mod。
 */

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();

    // Initialize DB
    initLogging();
    Settings settings = Settings::loadOrCreate();
    spdlog::info("Repo DB: {}", settings.repoDbPath);

    auto db = std::make_shared<Db>(settings.repoDbPath);
    runMigrations(*db);
    spdlog::info("Schema ready, version {}", migrations::currentSchemaVersion(*db));
    repo_ = std::make_unique<RepositoryService>(db);

    loadData();
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    table_ = new QTableWidget(0, 6, central);
    QStringList headers = {"ID", "名称", "评分", "分类ID", "大小(MB)", "文件路径"};
    table_->setHorizontalHeaderLabels(headers);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshBtn_ = new QPushButton("刷新", central);
    connect(refreshBtn_, &QPushButton::clicked, this, &MainWindow::onRefresh);

    layout->addWidget(table_);
    layout->addWidget(refreshBtn_);
    setCentralWidget(central);
    resize(900, 600);
    setWindowTitle("L4D2 MOD 管理器 - 极速MVP");
}

void MainWindow::onRefresh() { loadData(); }

void MainWindow::loadData() {
    auto rows = repo_->listVisible();
    table_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& m = rows[i];
        table_->setItem(i, 0, new QTableWidgetItem(QString::number(m.id)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(m.name)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::number(m.rating)));
        table_->setItem(i, 3, new QTableWidgetItem(QString::number(m.category_id)));
        table_->setItem(i, 4, new QTableWidgetItem(QString::number(m.size_mb, 'f', 2)));
        table_->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(m.file_path)));
    }
}
