param([switch]$English, [switch]$AllowUntested, [string]$ApplicationPath = 'C:\Program Files\DAZ 3D\DAZStudio6\DAZStudio.exe')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $English -and -not $AllowUntested -and (Test-Path -LiteralPath (Join-Path $root 'SAFETY-DISABLED.json'))) { throw '此候选版正在完成稳定性验收，请暂用英文启动入口。' }
if (-not (Test-Path -LiteralPath $ApplicationPath)) { throw "找不到 Daz Studio，请用 -ApplicationPath 指定主程序位置。" }
$info = Get-Content -LiteralPath (Join-Path $root 'build-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if (Get-Process -Name DAZStudio -ErrorAction SilentlyContinue) { throw 'Daz 已经打开。请使用窗口中的语言按钮切换，或先正常退出，再选择启动入口。' }
if (-not $English -and -not $AllowUntested) {
    $core = Join-Path (Split-Path -Parent $ApplicationPath) 'DzCore.dll'
    if ((Get-FileHash -LiteralPath $core -Algorithm SHA256).Hash.ToLowerInvariant() -ne $info.application_core_sha256) {
        throw 'Daz 主程序与已核对版本不同。请先运行兼容性检查，完成核对后更新 build-info.json。英文启动入口仍可正常使用。'
    }
    $gui = Join-Path (Split-Path -Parent $ApplicationPath) 'Qt6Gui.dll'
    if ((Get-FileHash -LiteralPath $gui -Algorithm SHA256).Hash.ToLowerInvariant() -ne $info.qt_gui_sha256) { throw '界面组件版本发生变化，请先重新核对兼容性。' }
    $widgets = Join-Path (Split-Path -Parent $ApplicationPath) 'Qt6Widgets.dll'
    if ((Get-FileHash -LiteralPath $widgets -Algorithm SHA256).Hash.ToLowerInvariant() -ne $info.qt_widgets_sha256) { throw '控件组件版本发生变化，请先重新核对兼容性。' }
}
$process = New-Object System.Diagnostics.ProcessStartInfo
$process.FileName = $ApplicationPath
$process.WorkingDirectory = Split-Path -Parent $ApplicationPath
$process.UseShellExecute = $false
foreach ($name in @('DAZ_ZHCN_ROOT','DAZ_ZHCN_TEST','DAZ_ZHCN_TRANSLATOR_ONLY')) { $process.EnvironmentVariables.Remove($name) }
$generic = @($process.EnvironmentVariables['QT_QPA_GENERIC_PLUGINS'] -split ',' | Where-Object { $_ -and $_ -ne 'dazzhcn' })
if (-not $English) {
    $settingsPath = Join-Path $root 'settings.json'
    $settings = if (Test-Path -LiteralPath $settingsPath) { Get-Content -LiteralPath $settingsPath -Raw -Encoding UTF8 | ConvertFrom-Json } else { [pscustomobject]@{chinese=$true;paint_adapter=$true} }
    $settings | Add-Member -NotePropertyName chinese -NotePropertyValue $true -Force
    [IO.File]::WriteAllText($settingsPath, ($settings | ConvertTo-Json), (New-Object Text.UTF8Encoding($false)))
    $process.EnvironmentVariables['DAZ_ZHCN_ROOT'] = $root
    $generic += 'dazzhcn'
    $paths = @((Join-Path $root 'runtime')) + @($process.EnvironmentVariables['QT_PLUGIN_PATH'] -split ';' | Where-Object { $_ })
    $process.EnvironmentVariables['QT_PLUGIN_PATH'] = ($paths | Select-Object -Unique) -join ';'
}
if ($generic.Count) { $process.EnvironmentVariables['QT_QPA_GENERIC_PLUGINS'] = $generic -join ',' } else { $process.EnvironmentVariables.Remove('QT_QPA_GENERIC_PLUGINS') }
[System.Diagnostics.Process]::Start($process) | Out-Null
