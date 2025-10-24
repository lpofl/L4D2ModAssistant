#pragma once

#include <QMainWindow>
#include <QString>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core/repo/RepositoryService.h"

class QStackedWidget;
class QLineEdit;
class QComboBox;
class QTableWidget;
class QPushButton;
class QLabel;
class QTextEdit;

class QSortFilterProxyModel;
class QStandardItemModel;

/**
 * @brief Application main window hosting repository/selector/settings tabs.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);

private slots:
  void onRefresh();
  void onImport();
  void onEdit();
  void onDelete();
  void onFilterChanged();
  void onFilterAttributeChanged(const QString& attribute);
  void onFilterValueTextChanged(const QString& text);
  void onCurrentRowChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);
  void switchToRepository();
  void switchToSelector();
  void switchToSettings();

private:
  void setupUi();
  QWidget* buildNavigationBar();
  QWidget* buildRepositoryPage();
  QWidget* buildSelectorPage();
  QWidget* buildSettingsPage();
  void reloadCategories();
  void reloadTags();
  void reloadAuthors();
  void reloadRatings();
  void loadData();
  void populateTable();
  void updateDetailForMod(int modId);
  QString categoryNameFor(int categoryId) const;
  QString tagsTextForMod(int modId);
  QString formatTagSummary(const std::vector<TagWithGroupRow>& rows,
                           const QString& groupSeparator,
                           const QString& tagSeparator) const;
  bool categoryMatchesFilter(int modCategoryId, int filterCategoryId) const;
  std::vector<TagDescriptor> tagsForMod(int modId) const;
  void updateTabButtonState(QPushButton* active);

  std::unique_ptr<RepositoryService> repo_;
  QString repoDir_;

  // Navigation
  QStackedWidget* stack_{};
  QPushButton* repoButton_{};
  QPushButton* selectorButton_{};
  QPushButton* settingsButton_{};

  // Repository page widgets
  QComboBox* filterAttribute_{};
  QComboBox* filterValue_{};
  QTableWidget* modTable_{};
  QPushButton* importBtn_{};
  QPushButton* editBtn_{};
  QPushButton* deleteBtn_{};
  QPushButton* refreshBtn_{};
  QLabel* coverLabel_{};
  QTextEdit* noteView_{};
  QLabel* metaLabel_{};

  // Data cache
  std::vector<ModRow> mods_;
  std::unordered_map<int, QString> categoryNames_;
  std::unordered_map<int, int> categoryParent_;
  std::unordered_map<int, QString> modTagsText_;
  std::unordered_map<int, std::vector<TagWithGroupRow>> modTagsCache_;

  // Filter model
  QSortFilterProxyModel* proxyModel_{};
  QStandardItemModel* filterModel_{};
};
