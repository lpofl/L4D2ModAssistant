#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <unordered_map>
#include <vector>

class QListWidgetItem;
class QTreeWidgetItem;
class SettingsPage;
class RepositoryService;
class ImportService;
struct Settings;

/**
 * @brief 设置页业务协调器，负责桥接 UI 控件与仓库服务。
 */
class SettingsPresenter : public QObject {
  Q_OBJECT
public:
  SettingsPresenter(SettingsPage* page,
                    Settings& settings,
                    RepositoryService* repo,
                    QWidget* dialogParent,
                    std::function<void()> repositoryReload,
                    QObject* parent = nullptr);

  void setRepositoryService(RepositoryService* repo);
  void refreshAll();

signals:
  void requestRepositoryReinitialize();

private slots:
  void onSettingsNavChanged(int row);
  void onBrowseRepoDir();
  void onBrowseGameDir();
  void onGameDirEdited(const QString& path);
  void onImportModeChanged(int index);
  void onAutoImportToggled(bool checked);
  void onAutoImportModeChanged(int index);
  void onSaveSettings();
  void onClearDeletedMods();

  void onCategorySelectionChanged();
  void onCategoryItemChanged(QTreeWidgetItem* item, int column);
  void onAddCategoryTopLevel();
  void onAddCategoryChild();
  void onRenameCategory();
  void onDeleteCategory();
  void onMoveCategoryUp();
  void onMoveCategoryDown();

  void onTagGroupSelectionChanged(int row);
  void onAddTagGroup();
  void onRenameTagGroup();
  void onDeleteTagGroup();
  void onTagSelectionChanged(int row);
  void onAddTag();
  void onRenameTag();
  void onDeleteTag();

private:
  int selectedCategoryId() const;
  int selectedCategoryParentId(int categoryId) const;
  int selectedTagGroupId() const;
  int selectedTagId() const;

  void refreshBasicSettingsUi();
  void refreshCategoryManagementUi();
  void refreshTagManagementUi();
  void refreshTagListForGroup(int groupId);
  void refreshDeletionSettingsUi();
  void ensureSettingsNavSelection();
  void setSettingsStatus(const QString& text, bool isError = false);
  void updateDerivedGamePaths(const QString& rootPath);

  QString normalizeRootInput(const QString& rawPath) const;
  QString deriveAddonsPath(const QString& rootPath) const;
  QString deriveWorkshopPath(const QString& addonsPath) const;

  SettingsPage* page_{};
  Settings* settings_{};
  RepositoryService* repo_{};
  QWidget* dialogParent_{};
  std::function<void()> repositoryReloadCallback_{};

  bool suppressCategoryItemSignals_ = false;
};
