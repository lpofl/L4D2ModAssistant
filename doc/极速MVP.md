🧩 一、目标定义（极速 MVP）
目标：
在 3–5 天内实现一个可运行的 Left 4 Dead 2 MOD 管理器最小可用版本，能导入 MOD、展示列表、编辑评分备注、同步到游戏目录。

MVP 不包含：

VPK/ZIP 解析

Steam 页面信息抓取

随机器

封面图缓存

多线程导入

🧱 二、极速 MVP 模块任务清单
模块	目标	文件/类	预计时间
项目初始化	Qt + SQLite 环境 + 架构搭建	main.cpp、CMakeLists.txt、vcpkg.json	0.5 天
数据库封装	连接管理、迁移、基本表创建	core/db/Db.h/.cpp、core/db/Stmt.h、core/db/Migrations.h	0.5 天
DAO 层	插入 / 查询 MOD	core/repo/RepositoryDao.h/.cpp	0.5 天
Service 层	封装导入、查询逻辑	core/repo/RepositoryService.h/.cpp	0.5 天
UI 层	Qt 主窗口、表格展示	app/ui/MainWindow.ui/.h/.cpp	1 天
导入功能	从文件/文件夹导入	core/import/ImportService.h/.cpp	0.5 天
同步功能	将选中 MOD 复制到游戏目录	core/sync/GameSyncService.h/.cpp	0.5 天
配置管理	settings.json 读写	core/config/Settings.h/.cpp	0.5 天
日志与异常	spdlog 控制台输出	core/log/Log.h/.cpp	0.5 天

预计总工期：3–5 天

📁 三、目录结构（初始版）
bash
复制代码
L4D2ModManager/
│
├─ app/
│  ├─ main.cpp
│  └─ ui/
│     ├─ MainWindow.h/.cpp/.ui
│
├─ core/
│  ├─ db/Db.h/.cpp
│  ├─ db/Stmt.h
│  ├─ db/Migrations.h
│  ├─ repo/RepositoryDao.h/.cpp
│  ├─ repo/RepositoryService.h/.cpp
│  ├─ import/ImportService.h/.cpp
│  ├─ sync/GameSyncService.h/.cpp
│  ├─ config/Settings.h/.cpp
│  ├─ log/Log.h/.cpp
│  └─ util/
│
├─ resources/
│  ├─ icons/
│  └─ default/
│
├─ sql/
│  └─ migrations/
│
├─ CMakeLists.txt
├─ vcpkg.json
└─ settings.json (首次启动生成)
⚙️ 四、首批代码骨架
我将为你生成以下关键文件的初版代码（全部可编译）：

CMakeLists.txt —— 项目配置

vcpkg.json —— 依赖定义（Qt6 + SQLite3 + spdlog + nlohmann/json）

core/db/Db.h / Stmt.h / Migrations.h —— SQLite 封装

app/main.cpp —— Qt 应用入口

app/ui/MainWindow.h/.cpp/.ui —— 主窗口 + 表格展示

core/repo/RepositoryDao.h/.cpp —— 基础数据库访问

core/repo/RepositoryService.h/.cpp —— 数据操作逻辑

