#pragma once

#include <QMainWindow>
#include <QString>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core/config/Settings.h"
#include "core/repo/RepositoryService.h"

class QStackedWidget;
class QLineEdit;
class QComboBox;
class QTableWidget;
class QPushButton;
class QLabel;
class QTextEdit;
class QListWidget;
class QTreeWidget;
class QListWidgetItem;

class QCheckBox;
class QTreeWidgetItem;
class QFrame;
class QScrollArea;

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
  void onSelectorFilterChanged();
  void onSelectorFilterAttributeChanged(const QString& attribute);
  void onSelectorFilterValueTextChanged(const QString& text);
  void onCurrentRowChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);
  void switchToRepository();
  void switchToSelector();
  void switchToSettings();

  // Selector page slots
  void onConfigureStrategy();
  void onRandomize();
  void onSaveCombination();
  void onApplyToGame();

  // Repository page slots
  void onShowDeletedModsToggled(bool checked);

  // Settings page slots
  void onClearDeletedMods();
  void onSettingsNavChanged(int row);
  void onBrowseRepoDir();
  void onBrowseGameRoot();
  void onGameRootEdited(const QString& path);
  void onImportModeChanged(int index);
  void onAutoImportToggled(bool checked);
  void onAutoImportModeChanged(int index);
  void onSaveSettings();
  void onCategorySelectionChanged();
  void onAddCategoryTopLevel();
  void onAddCategoryChild();
  void onRenameCategory();
  void onDeleteCategory();
  void onCategoryItemChanged(QTreeWidgetItem* item, int column);
  void onMoveCategoryUp();
  void onMoveCategoryDown();
  void onTagGroupSelectionChanged(int row);
  void onAddTagGroup();
  void onRenameTagGroup();
  void onDeleteTagGroup();
  void onAddTag();
  void onRenameTag();
  void onDeleteTag();
  void onTagSelectionChanged(int row);

private:
  void setupUi();
  QWidget* buildNavigationBar();
  QWidget* buildRepositoryPage();
  QWidget* buildSelectorPage();
  QWidget* buildSettingsPage();
  QWidget* buildBasicSettingsPane();
  QWidget* buildCategoryManagementPane();
  QWidget* buildTagManagementPane();
  QWidget* buildDeletionPane();
  void reloadCategories();
  void reloadTags();
  void reloadAuthors();
  void reloadRatings();
  void reloadRepoSelectorData();
  void applySelectorFilter();
  void populateCategoryFilterModel(QStandardItemModel* model, bool updateCache);
  void populateTagFilterModel(QStandardItemModel* model) const;
  void populateAuthorFilterModel(QStandardItemModel* model) const;
  void populateRatingFilterModel(QStandardItemModel* model) const;
  void loadData();
  void populateTable();
  void updateDetailForMod(int modId);
  bool modMatchesFilter(const ModRow& mod,
                        const QString& attribute,
                        int filterId,
                        const QString& filterValue) const;
  int filterIdForCombo(const QComboBox* combo,
                       const QSortFilterProxyModel* proxy,
                       const QStandardItemModel* model) const;
  QString categoryNameFor(int categoryId) const;
  QString tagsTextForMod(int modId);
  QString formatTagSummary(const std::vector<TagWithGroupRow>& rows,
                           const QString& groupSeparator,
                           const QString& tagSeparator) const;
  bool categoryMatchesFilter(int modCategoryId, int filterCategoryId) const;
  std::vector<TagDescriptor> tagsForMod(int modId) const;
  void refreshBasicSettingsUi();
  void refreshCategoryManagementUi();
  void refreshTagManagementUi();
  void refreshTagListForGroup(int groupId);
  void refreshDeletionSettingsUi();
  void ensureSettingsNavSelection();
  void setSettingsStatus(const QString& text, bool isError = false);
  void reinitializeRepository(const Settings& settings);
  int selectedCategoryId() const;
  void adjustCategoryOrder(int direction);
  int selectedTagGroupId() const;
  int selectedTagId() const;
  void updateTabButtonState(QPushButton* active);
  QString detectL4D2GameDirectory() const; // New helper method
  QString normalizeAddonsPath(const QString& path) const; // 归一化 addons 路径，确保指向 addons 目录
  void updateGamePathDisplays(const QString& rootPath); // 根据根目录更新 addons/workshop 展示路径
  QString deriveGameRootFromAddons(const QString& addonsPath) const; // 从 addons 路径推导游戏根目录

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
  QCheckBox* showDeletedModsCheckBox_{};

  // Selector page widgets
  QTableWidget* gameDirTable_{};
  QTableWidget* repoTable_{};
  QComboBox* selectorFilterAttribute_{};
  QComboBox* selectorFilterValue_{};
  QPushButton* configureStrategyBtn_{};
  QPushButton* randomizeBtn_{};
  QPushButton* saveCombinationBtn_{};
  QPushButton* applyToGameBtn_{};
  QLabel* strategyInfoLabel_{};

  // Settings page widgets
  QListWidget* settingsNav_{};
  QStackedWidget* settingsStack_{};
  QLineEdit* settingsRepoDirEdit_{};
  QPushButton* settingsRepoBrowseBtn_{};
  QLineEdit* settingsGameRootEdit_{};
  QPushButton* settingsGameRootBrowseBtn_{};
  QLineEdit* settingsAddonsPathDisplay_{};
  QLineEdit* settingsWorkshopPathDisplay_{};
  QComboBox* importModeCombo_{};
  QCheckBox* autoImportCheckbox_{};
  QComboBox* autoImportModeCombo_{};
  QPushButton* saveSettingsBtn_{};
  QLabel* settingsStatusLabel_{};

  QTreeWidget* categoryTree_{};
  QPushButton* categoryAddRootBtn_{};
  QPushButton* categoryAddChildBtn_{};
  QPushButton* categoryRenameBtn_{};
  QPushButton* categoryDeleteBtn_{};
  QPushButton* categoryMoveUpBtn_{};
  QPushButton* categoryMoveDownBtn_{};

  QListWidget* tagGroupList_{};
  QListWidget* tagList_{};
  QPushButton* tagGroupAddBtn_{};
  QPushButton* tagGroupRenameBtn_{};
  QPushButton* tagGroupDeleteBtn_{};
  QPushButton* tagAddBtn_{};
  QPushButton* tagRenameBtn_{};
  QPushButton* tagDeleteBtn_{};

  QCheckBox* retainDeletedCheckbox_{};
  QPushButton* clearDeletedModsBtn_{};

  // Data cache
  std::vector<ModRow> mods_;
  std::unordered_map<int, QString> categoryNames_;
  std::unordered_map<int, int> categoryParent_;
  std::unordered_map<int, QString> modTagsText_;
  std::unordered_map<int, std::vector<TagWithGroupRow>> modTagsCache_;

  // Filter model
  QSortFilterProxyModel* proxyModel_{};
  QStandardItemModel* filterModel_{};

  // Selector filter model
  QSortFilterProxyModel* selectorProxyModel_{};
  QStandardItemModel* selectorFilterModel_{};

  Settings settings_{};
  bool suppressCategoryItemSignals_ = false;
};
