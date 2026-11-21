# 第三方组件与资料

- Qt 官方简体中文翻译：Qt 6.10.3 的 qttranslations，选择其 BSD-3-Clause 授权使用。随包提供所使用翻译的 `.ts` 源文件、版权说明和许可文本，官方 SBOM 中的许可记录也一并保留。运行时使用 Daz 已有的 Qt 动态库。
- Qt 插件和翻译接口资料：QGenericPlugin、QGuiApplication 的 `-plugin` / `QT_QPA_GENERIC_PLUGINS`、QTranslator。具体来源见 `reports/官方接口来源.md`。
- 离线初译模型：OPUS-MT 英语→中文，Argos Translate 1.9 封装。原模型作者 Jörg Tiedemann、Santhosh Thottingal，原模型许可 CC BY 4.0。模型仅用于生成词库，不作为语言包运行依赖，也不放进运行安装包。
- 本插件和本任务编写的安装、构建工具：MIT，见 `LICENSE.txt`。
- 自绘界面适配使用 MinHook 1.3.4，采用其 BSD-2-Clause 许可。原始源码、版权说明与许可保存在 `source/third_party/minhook`，下载来源和校验值保存在来源记录中。适配仅在 Daz 自身进程内运行，不改写官方 DLL 文件。
- Daz Studio、原来的内置资源与商标属于各自权利人。语言包不附带 Daz 主程序、SDK 或原素材。
