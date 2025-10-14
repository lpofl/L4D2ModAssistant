
#pragma once
#include <QMainWindow>
#include <memory>

class QTableWidget;
class QPushButton;

class RepositoryService;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
private slots:
    void onRefresh();
private:
    void setupUi();
    void loadData();
    QTableWidget* table_;
    QPushButton* refreshBtn_;
    std::unique_ptr<RepositoryService> repo_;
};
