#pragma once

#include <QWidget>

class QListWidget;
class QStackedWidget;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QLabel;
class QTreeWidget;
class QListWidget;

/**
 * @brief 设置页界面聚合控件，提供统一的 UI 访问入口。
 */
class SettingsPage : public QWidget {
  Q_OBJECT
public:
  explicit SettingsPage(QWidget* parent = nullptr);

  QListWidget* navigation() const { return settingsNav_; }
  QStackedWidget* stack() const { return settingsStack_; }

  // 基础设置控件
  QLineEdit* repoDirEdit() const { return settingsRepoDirEdit_; }
  QPushButton* repoBrowseButton() const { return settingsRepoBrowseBtn_; }
  QLineEdit* gameDirEdit() const { return settingsGameDirEdit_; }
  QPushButton* gameDirBrowseButton() const { return settingsGameDirBrowseBtn_; }
  QLineEdit* addonsDisplay() const { return settingsAddonsPathDisplay_; }
  QLineEdit* workshopDisplay() const { return settingsWorkshopPathDisplay_; }
  QComboBox* importModeCombo() const { return importModeCombo_; }
  QCheckBox* autoImportCheck() const { return autoImportCheckbox_; }
  QComboBox* autoImportModeCombo() const { return autoImportModeCombo_; }
  QPushButton* saveSettingsButton() const { return saveSettingsBtn_; }
  QLabel* statusLabel() const { return settingsStatusLabel_; }

  // 分类管理控件
  QTreeWidget* categoryTree() const { return categoryTree_; }
  QPushButton* categoryAddRootButton() const { return categoryAddRootBtn_; }
  QPushButton* categoryAddChildButton() const { return categoryAddChildBtn_; }
  QPushButton* categoryRenameButton() const { return categoryRenameBtn_; }
  QPushButton* categoryDeleteButton() const { return categoryDeleteBtn_; }
  QPushButton* categoryMoveUpButton() const { return categoryMoveUpBtn_; }
  QPushButton* categoryMoveDownButton() const { return categoryMoveDownBtn_; }

  // 标签管理控件
  QListWidget* tagGroupList() const { return tagGroupList_; }
  QListWidget* tagList() const { return tagList_; }
  QPushButton* tagGroupAddButton() const { return tagGroupAddBtn_; }
  QPushButton* tagGroupRenameButton() const { return tagGroupRenameBtn_; }
  QPushButton* tagGroupDeleteButton() const { return tagGroupDeleteBtn_; }
  QPushButton* tagAddButton() const { return tagAddBtn_; }
  QPushButton* tagRenameButton() const { return tagRenameBtn_; }
  QPushButton* tagDeleteButton() const { return tagDeleteBtn_; }

  // 删除策略控件
  QCheckBox* retainDeletedCheck() const { return retainDeletedCheckbox_; }
  QPushButton* clearDeletedButton() const { return clearDeletedModsBtn_; }

private:
  QWidget* buildBasicSettingsPane();
  QWidget* buildCategoryManagementPane();
  QWidget* buildTagManagementPane();
  QWidget* buildDeletionPane();

  QListWidget* settingsNav_{};
  QStackedWidget* settingsStack_{};

  // Basic pane
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

  // Category pane
  QTreeWidget* categoryTree_{};
  QPushButton* categoryAddRootBtn_{};
  QPushButton* categoryAddChildBtn_{};
  QPushButton* categoryRenameBtn_{};
  QPushButton* categoryDeleteBtn_{};
  QPushButton* categoryMoveUpBtn_{};
  QPushButton* categoryMoveDownBtn_{};

  // Tag pane
  QListWidget* tagGroupList_{};
  QListWidget* tagList_{};
  QPushButton* tagGroupAddBtn_{};
  QPushButton* tagGroupRenameBtn_{};
  QPushButton* tagGroupDeleteBtn_{};
  QPushButton* tagAddBtn_{};
  QPushButton* tagRenameBtn_{};
  QPushButton* tagDeleteBtn_{};

  // Deletion pane
  QCheckBox* retainDeletedCheckbox_{};
  QPushButton* clearDeletedModsBtn_{};
};

