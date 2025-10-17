
#pragma once
#include <QMainWindow>
#include <memory>
#include "core/repo/RepositoryService.h"

/**
 * @file MainWindow.h
 * @brief 应用主窗口，负责展示仓库可见 Mod 列表。
 */

class QTableWidget;
class QPushButton;

/**
 * @brief 主窗口类，包含基础的表格展示与刷新逻辑。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /**
     * @brief 构造函数。
     * @param parent 父窗口。
     */
    explicit MainWindow(QWidget* parent = nullptr);
private slots:
    /** @brief 点击“刷新”按钮时回调，重新加载数据。 */
    void onRefresh();
private:
    /** @brief 初始化 UI 控件与布局。 */
    void setupUi();
    /** @brief 从仓库加载数据并填充表格。 */
    void loadData();

    QTableWidget* table_{};              ///< 数据表格。
    QPushButton* refreshBtn_{};          ///< 刷新按钮。
    std::unique_ptr<RepositoryService> repo_; ///< 仓库服务。
};
