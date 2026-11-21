param([string]$ApplicationDirectory = 'C:\Program Files\DAZ 3D\DAZStudio6')
$ErrorActionPreference = 'Stop'
$info = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$hash = (Get-FileHash -LiteralPath (Join-Path $ApplicationDirectory 'DzCore.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$qt = (Get-Item -LiteralPath (Join-Path $ApplicationDirectory 'Qt6Core.dll')).VersionInfo.FileVersion
$guiHash = (Get-FileHash -LiteralPath (Join-Path $ApplicationDirectory 'Qt6Gui.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$widgetsHash = (Get-FileHash -LiteralPath (Join-Path $ApplicationDirectory 'Qt6Widgets.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
[pscustomobject]@{TargetDaz=$info.target_daz;InstalledQt=$qt;TargetQt=$info.qt;SameDazCore=($hash -eq $info.application_core_sha256);SameQtGui=($guiHash -eq $info.qt_gui_sha256);SameQtWidgets=($widgetsHash -eq $info.qt_widgets_sha256);Architecture='x64';PluginType='Qt language extension'} | Format-List
if ($hash -eq $info.application_core_sha256 -and $guiHash -eq $info.qt_gui_sha256 -and $widgetsHash -eq $info.qt_widgets_sha256) { Write-Host '程序文件与构建目标一致。实际验收状态请查看随包记录。' } else { Write-Host '主程序已变化，请按说明重新核对界面、保存、重开和导出；Qt 版本变化时重新编译。' }
