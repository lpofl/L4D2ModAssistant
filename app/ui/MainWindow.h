#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core/config/Settings.h"
#include "core/repo/RepositoryService.h"
// 界面瘦身：引入应用服务/控制器头文件
#include "app/services/GameDirectoryMonitor.h"
#include "app/services/ImportService.h"
#include "app/ui/selector/RandomizeController.h"

class QStackedWidget;
class QLineEdit;
class QComboBox;
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
class NavigationBar;
class ModFilterPanel;
class ModTableWidget;
class RepositoryPage;
class RepositoryPresenter;
class SelectorPage;
class SelectorPresenter;
class SettingsPage;

/**
 * @brief Application main window hosting repository/selector/settings tabs.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

private slots:
  void switchToRepository();
  void switchToSelector();
  void switchToSettings();

  // Selector page slots
  void onConfigureStrategy();
  void onRandomize();
  void onSaveCombination();
  void onApplyToGame();

  // Settings page slots
  void onClearDeletedMods();
  void onSettingsNavChanged(int row);
  void onBrowseRepoDir();
  void onBrowseGameDir();
  void onGameDirEdited(const QString& path);
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
  void onGameModsUpdated(const QStringList& updatedMods, bool initialScan);

private:
  void setupUi();
  void onRepositoryModsReloaded();
  void reloadRepoSelectorData();
  void applySelectorFilter();
  void populateCategoryFilterModel(QStandardItemModel* model, bool updateCache);
  void populateTagFilterModel(QStandardItemModel* model) const;
  void populateAuthorFilterModel(QStandardItemModel* model) const;
  void populateRatingFilterModel(QStandardItemModel* model) const;
  void refreshBasicSettingsUi();
  void refreshCategoryManagementUi();
  void refreshTagManagementUi();
  void refreshTagListForGroup(int groupId);
  void refreshDeletionSettingsUi();
  void ensureSettingsNavSelection();
  void setSettingsStatus(const QString& text, bool isError = false);
  void reinitializeRepository(const Settings& settings);
  void scheduleGameDirectoryScan(bool showOverlay);
  int selectedCategoryId() const;
  void adjustCategoryOrder(int direction);
  int selectedTagGroupId() const;
  int selectedTagId() const;
  QString detectL4D2GameDirectory() const; // New helper method
  QString deriveAddonsPath(const QString& rootPath) const; // 从根目录推导 addons 目录
  QString deriveWorkshopPath(const QString& addonsPath) const; // 从 addons 目录推导 workshop 目录
  void updateDerivedGamePaths(const QString& rootPath); // 根据根目录刷新展示
  QString normalizeRootInput(const QString& rawPath) const; // 统一清理用户输入的根目录
  bool ensureModFilesInRepository(ModRow& mod, QStringList& errors) const; // 按入库方式处理 MOD 文件与封面

  std::unique_ptr<RepositoryService> repo_;
  QString repoDir_;
  // UI 层应用服务与控制器（减少 MainWindow 职责）
  std::unique_ptr<ImportService> importService_;
  std::unique_ptr<RandomizeController> randomizeController_;
  std::unique_ptr<GameDirectoryMonitor> gameDirectoryMonitor_;

  // Navigation
  NavigationBar* navigationBar_{};
  QStackedWidget* stack_{};

  // Repository page widgets
  RepositoryPage* repositoryPage_{};
  std::unique_ptr<RepositoryPresenter> repositoryPresenter_;
  ModFilterPanel* repoFilterPanel_{};
  QComboBox* filterAttribute_{}; // owned by repoFilterPanel_
  QComboBox* filterValue_{};     // owned by repoFilterPanel_
  ModTableWidget* modTable_{};
  QPushButton* importBtn_{};
  QPushButton* editBtn_{};
  QPushButton* deleteBtn_{};
  QPushButton* refreshBtn_{};
  QLabel* coverLabel_{};
  QTextEdit* noteView_{};
  QLabel* metaLabel_{};
  QCheckBox* showDeletedModsCheckBox_{};

  // Selector page widgets
  SelectorPage* selectorPage_{};
  std::unique_ptr<SelectorPresenter> selectorPresenter_;
  QPushButton* configureStrategyBtn_{};
  QPushButton* randomizeBtn_{};
  QPushButton* saveCombinationBtn_{};
  QPushButton* applyToGameBtn_{};
  QLabel* strategyInfoLabel_{};

  // Settings page widgets
  SettingsPage* settingsPage_{};
  QListWidget* settingsNav_{};
  QStackedWidget* settingsStack_{};
  QLineEdit* settingsRepoDirEdit_{};
  QPushButton* settingsRepoBrowseBtn_{};
  QLineEdit* settingsGameDirEdit_{};
  QPushButton* settingsGameDirBrowseBtn_{};
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

  Settings settings_{};
  bool suppressCategoryItemSignals_ = false;
  bool isGameModsLoading_ = false;
};


