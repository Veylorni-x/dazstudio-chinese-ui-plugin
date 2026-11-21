# Daz Studio 简体中文语言包

目标程序：**Daz Studio 6.26.2026.30901，Windows 64 位**。默认主程序位置：`C:\Program Files\DAZ 3D\DAZStudio6\DAZStudio.exe`。

这是采用 **Daz Studio 6.26 原生 SDK 插件接口（`DzPlugin` ABI，SDK `6.26.2026.30200`）+ Qt `QGenericPlugin` 双模架构** 的简体中文语言插件。插件既可放入 Daz Studio 的 `plugins/` 目录由 `DzPluginMgr` 原生加载（并在“帮助 → 关于已安装插件”中显示），也可免管理员权限通过用户目录启动入口直接加载并自动注册至 `DzPluginMgr`。汉化程序、可编辑词库和源码分开保存。日常使用只依赖 Daz 已有的界面组件，不需要 Python、翻译模型、联网服务或付费汉化插件。

## 安装和开始使用

1. 正常关闭 Daz Studio。
2. 双击本目录的 **安装汉化.cmd**。
3. 安装完成后，使用桌面的 **Daz Studio 简体中文.cmd** 启动。
4. 软件完成启动后，工具栏会出现 **语言 / Language**。在这里可以切换简体中文、English、重新加载词库，以及查看离线中文帮助。

默认安装位置：`%LOCALAPPDATA%\DazStudioZhCN`。也可以直接使用本目录的“启动中文版.cmd”，以便携方式运行。

安装过程复制本语言包，保留 Daz 主程序、原插件、素材和现有场景。安装前会把现有界面设置文件备份到本目录的 `reports/install-backup-时间`。整个方案使用真实文件与普通复制，不创建文件链接。

**Daz 已经打开时，直接使用窗口中的语言按钮切换。更换启动入口之前先正常退出软件。**

## 回到英文和卸载

- 在当前窗口切回英文：语言 / Language → English。
- 以原版英文方式启动：正常退出后使用“Daz Studio 英文.cmd”或原来的 Daz 启动入口。
- 卸载已安装副本：关闭 Daz，运行安装目录内的“卸载汉化.cmd”。自定义词库会先备份到“文档”中的 `DazStudioZhCN词库备份`。
- 便携副本：关闭 Daz 后移除该副本即可。源码与开发工具可以单独保留。

## 翻译怎样保持原始数据

本包不改写场景、人物、材质、动作或素材数据库。参数名称、节点原名、文件路径、骨骼标识和搜索数据继续由 Daz 原来的数据管理。

素材与参数列表使用显示时的文字替换，原来的列表数据和编辑操作保持原样。可编辑的文件名、路径、脚本代码和数值输入不作为普通文字翻译。原来的英文素材搜索和脚本标识继续使用原值。

Qt 标准对话框使用 Qt 官方简体中文语言文件。Daz 的菜单、按钮、面板、提示和帮助文字由界面扩展补齐。程序内部在创建对象和参数时使用的字符串，不经过这层通用翻译。

自绘参数和原生控件在绘制阶段使用译文副本，保留 Daz 原来的样式、列表数据、编辑器和动作对象。界面素材名称的绘制副本与原名称分开。视口专用文字使用 `display-policy.json` 中限定的界面上下文，原始节点和参数数据类不在此名单中。

## 词库文件

| 文件 | 用途 |
| --- | --- |
| `dictionary/overrides.json` | 用户修订，优先级最高，更新语言包时保留 |
| `dictionary/glossary.tsv` | 统一术语，英文和中文之间用一个制表符分隔 |
| `dictionary/zh_CN.json` | 全量主词库，UTF-8 编码 |
| `dictionary/display-policy.json` | 视口界面上下文和方向文字等显示策略 |
| `runtime/*_zh_CN.qm` | Qt 官方简体中文标准界面语言文件 |

普通漏翻优先修改 `overrides.json`，例如：

```json
{
  "strings": {
    "Render Settings": "渲染设置",
    "Could not open %1": "无法打开 %1"
  },
  "contexts": {}
}
```

保存后，在 Daz 的语言按钮里选择“重新加载词库”。无须重新编译插件。

注意原文里的 `%1`、`%2`、`%n` 等位置标记。它们由软件填入名称或数值，译文应完整保留。HTML 帮助文字中的标签和链接也应保留。主词库里的错误提示模板会自动匹配相应的实际文件名或数值。

需要区分同一句话在不同地方的用法时，`contexts` 的键写成“上下文 + `\u0004` + 原文”。上下文可以在漏翻清单中找到。

## 范围与验收记录

词库来自本机安装的程序、内置插件、帮助与操作提示资源、着色器说明，以及默认资源、Genesis 9 入门资源和 dForce 入门资源中的显示名称、分类和参数。

术语表为人工整理；大批量长提示、说明与素材条目还包含离线翻译模型生成的初稿。实际核对结果以 `reports/验收结果.md` 和对应的验收记录为准。词条数量是词库规模，不作为界面验收结论。

“语言 → 导出漏翻清单”会写出 `reports/coverage-runtime.json`。这份清单保存在本机，不发送到外部服务。记录包括原文、上下文和是否匹配译文。实测仍为英文的位置列在 `reports/验收结果.md`。

帮助按钮中的“离线中文帮助”随包保存。本地安装的简介和元数据帮助另有中文副本。官方网站、论坛、在线文档和视频入口仍使用其原网站。

## 交付目录

```text
runtime/                 插件 DLL 和 Qt 中文语言文件
dictionary/              主词库、统一术语和用户修订
source/                  完整 C++ 源码、Qt 工程和 CMake 工程
help/                    离线中文操作说明与本地帮助
tools/                   构建、收集文字、生成词库和打包工具
reports/                 范围、来源、备份与验收记录
Start.ps1                中文 / 英文启动器
Install.ps1              安装脚本
Uninstall.ps1            卸载脚本
Check-Compatibility.ps1  版本检查
build-info.json          对应的主程序、Qt 与插件校验信息
```

安装包只包含运行、源码和说明需要的文件。开发过程中下载的编译器、翻译模型、缓存和测试场景不放进运行安装包。

## Daz 升级后怎样处理

1. 先保留 `overrides.json` 和 `glossary.tsv`。
2. 运行“检查兼容性.cmd”。主程序校验信息一致时，可以继续使用已核对的语言包。
3. 主程序发生变化时，在独立测试配置中重新检查语言切换、菜单和主要面板，再完成“加载角色 → 修改参数 → 保存 → 重开 → 导出”的流程。
4. 只有新增文字时，更新词库即可。
5. Qt 的主版本或次版本变化时，使用相同版本的 Qt 开发包重新编译插件。当前对应 Qt **6.10.3，MSVC 2022，x64**。自绘适配还核对 Daz 核心、Qt6Gui 与 Qt6Widgets 文件的校验值。
6. 完成新版本验收后，再更新 `build-info.json` 和验收记录。

开发者核对新版本时，可以显式使用 `Start.ps1 -AllowUntested`。正常启动入口会提示主程序版本变化，方便避免把旧版的验收记录混用于新版。

## 重新编译插件与原生 SDK 架构

本插件同时实现了 **Daz Studio 6.26 原生 SDK 插件 ABI**（导出 `.dz_sdk` 版本标签段 `dzPluginSDKVersionTag`、`getSDKVersion`、`getPluginDefinition`，构建 `DzPlugin` 实例并注册到 `DzPluginMgr`）以及 **Qt 官方 `QGenericPlugin` 接口**：

- **原生 `plugins/` 目录加载**：当以管理员权限运行 `Install.ps1`（或手动将 `runtime/generic/dazzhcn.dll` 复制到 `C:\Program Files\DAZ 3D\DAZStudio6\plugins\dazzhcn.dll`，并在同目录放置内容为语言包根目录路径的 `dazzhcn-root.txt`）时，Daz Studio 启动时由 `DzPluginMgr` 原生直接加载。
- **免管理员便携/用户目录加载**：普通用户直接双击 `安装汉化.cmd` 安装到 `%LOCALAPPDATA%\DazStudioZhCN` 并通过桌面 `Daz Studio 简体中文.cmd` 启动时，插件无需写入 `C:\Program Files`，并在主界面就绪后自动向 `DzPluginMgr` 注册原生 `DzPlugin` 实例，同样可在“帮助 → 关于已安装插件”或通过 `App.getPluginMgr().findPlugin("Daz Studio 简体中文 (Simplified Chinese)")` 查看到已加载状态。

自绘补充模块使用 MinHook，在本程序内临时适配 Qt 的绘图入口、Daz 原生样式绘制入口（`DzStyle::drawControl` / `DzStyle::drawPrimitive`）以及 3D 视口相机方块贴图绘制入口（`DzCameraCubeCombinedTexture::paint`）。匹配到的界面文字（包括 3D 视口右上角 `透视视图` 及方位方块 `正面 / 背面 / 左侧 / 右侧 / 顶面 / 底面`）以副本形式绘制，控件样式对象保持原样；输入框、脚本代码以及素材图像处理不作为文字替换对象。对话框标题和报错正文另作界面文字处理。官方程序文件不会被改写，退出进程后该适配随进程结束。完整适配代码和第三方库源码一并提供。

需要的免费工具：

- Microsoft Visual Studio 2022 Build Tools 的 C++ 编译工具和 Windows SDK。
- 与 Daz 对应的 Qt 6.10.3，MSVC 2022 x64 开发包。
- Python 3，或 CMake 3.21 及以上。

本机开发目录保留了本任务实际使用的编译工具，可以离线重新编译：

```powershell
python tools/build_plugin.py
```

若使用自己安装的工具，指定 Qt、MSVC 和 Windows SDK 根目录：

```powershell
python tools/build_plugin.py --qt-root "C:/Qt/6.10.3/msvc2022_64" --vc-root "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.xx.xxxxx" --sdk-root "C:/Program Files (x86)/Windows Kits/10"
```

也可以使用源码里的 CMake 工程：

```powershell
cmake -S source -B build-cmake -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.3/msvc2022_64"
cmake --build build-cmake --config Release
```

输出文件为 `runtime/generic/dazzhcn.dll`。重新编译前正常关闭加载了该 DLL 的 Daz 实例。

本机核对的 Daz 6 已公开 `-language`、`DzApp.uiLanguage` 和 SDK 中的 `getUiLanguage/setUiLanguage`。本机这版没有随附可供选择的简体中文 Daz 语言目录，因此语言插件采用 Daz 6.26 原生 `DzPlugin` + Qt 官方翻译文件与显示层扩展；不把 Daz 对象构造字符串作为通用翻译目标。

## 重新生成词库和打包

自己补充几个词时，只修改 `overrides.json`。更新 Daz 或素材后，需要重新收集时：

```powershell
python tools/collect_full_catalog.py
python tools/collect_renderer_messages.py
python tools/build_dictionary.py
python tools/build_local_help.py
python tools/package_release.py
```

全量词库生成工具使用本地的 CTranslate2 与 OPUS-MT 英中模型；这只是开发阶段的辅助工具。主程序、插件和已生成的词库日常运行不调用模型。模型来源、版本与校验值保存在开发目录的来源记录中。

打包工具会生成运行安装包和完整源码包，排除测试场景、个人界面备份、日志、编译缓存和翻译模型。

## 原文件与数据备份

主程序和官方插件按原路径使用，没有覆盖官方 DLL。素材按原路径读取，没有复制整套贴图，也没有给场景、节点、参数、文件夹或素材数据库改名。

首次测试使用单独的 Daz 实例配置。用于验收的场景和导出文件保存在本语言包的 `reports` 内，与原素材分开。

如果需要恢复安装前的界面布局，正常关闭 Daz 后，把安装备份中的对应界面文件复制回原 Daz 用户设置目录。安装脚本会打印该备份的实际位置。
