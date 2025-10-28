// UTF-8
#include "app/services/ImportService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

/**
 * 该实现与原 MainWindow::ensureModFilesInRepository 逻辑等价，
 * 仅将界面无关的文件处理职责迁移到 ImportService 中，便于复用与测试。
 */
bool ImportService::ensureModFilesInRepository(const Settings& settings, ModRow& mod, QStringList& errors) const {
  const ImportAction action = settings.importAction;
  if (action == ImportAction::None) {
    return true; // 不做导入处理
  }

  const QString repoDir = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(settings.repoDir)));
  if (repoDir.isEmpty()) {
    errors << QObject::tr("仓库目录未配置，无法执行导入处理");
    return false;
  }

  QDir repoDirObj(repoDir);
  if (!repoDirObj.exists() && !repoDirObj.mkpath(QStringLiteral("."))) {
    errors << QObject::tr("无法创建仓库目录：%1").arg(repoDir);
    return false;
  }

  const auto repoPrefix = [repoDirObj]() {
    QString prefix = QDir::fromNativeSeparators(repoDirObj.absolutePath());
    if (!prefix.endsWith(u'/')) {
      prefix.append(u'/');
    }
    return prefix;
  }();

  const auto allocateTargetPath = [&repoDirObj](const QFileInfo& sourceInfo) {
    const QString originalName = sourceInfo.fileName().isEmpty() ? QStringLiteral("mod") : sourceInfo.fileName();
    QString baseName = sourceInfo.completeBaseName();
    if (baseName.isEmpty()) {
      baseName = QStringLiteral("mod");
    }
    const QString suffix = sourceInfo.completeSuffix();

    QString candidateName = originalName;
    QString candidatePath = repoDirObj.filePath(candidateName);
    int counter = 1;
    while (QFileInfo::exists(candidatePath)) {
      if (suffix.isEmpty()) {
        candidateName = QStringLiteral("%1_%2").arg(baseName).arg(counter);
      } else {
        candidateName = QStringLiteral("%1_%2.%3").arg(baseName).arg(counter).arg(suffix);
      }
      candidatePath = repoDirObj.filePath(candidateName);
      ++counter;
    }
    return QDir::cleanPath(candidatePath);
  };

  const auto inRepo = [&repoPrefix](const QFileInfo& info) {
    const QString normalizedFile = QDir::fromNativeSeparators(info.absoluteFilePath());
    return normalizedFile.startsWith(repoPrefix, Qt::CaseInsensitive);
  };

  const auto performTransfer = [action, &errors](const QString& src, const QString& dst, const QString& label) {
    auto emitFailure = [&]() {
      errors << QObject::tr("无法%1 %2 到仓库目录：%3")
                    .arg(action == ImportAction::Cut ? QObject::tr("剪切") : QObject::tr("复制"))
                    .arg(label, dst);
      return false;
    };

    switch (action) {
      case ImportAction::Copy:
        if (!QFile::copy(src, dst)) {
          return emitFailure();
        }
        return true;
      case ImportAction::Cut:
        if (QFile::rename(src, dst)) {
          return true;
        }
        if (QFile::copy(src, dst)) {
          QFile::remove(src);
          return true;
        }
        return emitFailure();
      case ImportAction::None:
        return true;
    }
    return true;
  };

  const auto handlePath = [&](std::string& pathRef, const QString& label, bool required) {
    if (pathRef.empty()) {
      if (required) {
        errors << QObject::tr("%1路径为空，无法执行导入处理").arg(label);
        return false;
      }
      return true;
    }

    QString sourcePath = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(pathRef)));
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
      if (required) {
        errors << QObject::tr("找不到%1：%2").arg(label, sourcePath);
        return false;
      }
      return true;
    }

    if (inRepo(sourceInfo)) {
      // 已在仓库中，仅标准化存储
      pathRef = QDir::toNativeSeparators(sourceInfo.absoluteFilePath()).toStdString();
      return true;
    }

    const QString targetPath = allocateTargetPath(sourceInfo);
    QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !targetInfo.dir().mkpath(QStringLiteral("."))) {
      errors << QObject::tr("无法创建目标目录：%1").arg(targetInfo.dir().absolutePath());
      return false;
    }

    if (!performTransfer(sourceInfo.absoluteFilePath(), targetPath, label)) {
      return false;
    }

    pathRef = QDir::toNativeSeparators(targetPath).toStdString();
    return true;
  };

  const bool fileOk = handlePath(mod.file_path, QObject::tr("MOD 文件"), true);
  const bool coverOk = handlePath(mod.cover_path, QObject::tr("封面文件"), false);
  return fileOk && coverOk;
}

