param([string]$Destination = (Join-Path $env:LOCALAPPDATA 'DazStudioZhCN'), [switch]$NoDesktop, [switch]$AllowUntested, [string]$DazPluginsPath = 'C:\Program Files\DAZ 3D\DAZStudio6\plugins')
$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
if (-not $AllowUntested -and (Test-Path -LiteralPath (Join-Path $source 'SAFETY-DISABLED.json'))) { throw '此候选版正在完成稳定性验收，正式安装将在验收后开放。' }
if (Get-Process -Name DAZStudio -ErrorAction SilentlyContinue) { throw '请先正常关闭 Daz Studio，再安装或更新汉化。' }
$dest = [System.IO.Path]::GetFullPath($Destination)
if ($dest.TrimEnd('\') -eq $source.TrimEnd('\')) { throw '安装位置与源码目录相同，请直接使用本目录的启动入口。' }
$manifestPath = Join-Path $dest 'installation.json'
if ((Test-Path -LiteralPath $dest) -and -not (Test-Path -LiteralPath $manifestPath)) { throw '目标文件夹已经存在且不属于本汉化，请选择空文件夹。' }
$previousManifest = if (Test-Path -LiteralPath $manifestPath) { Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json } else { $null }
$backup = Join-Path $source ('reports\install-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
if (Test-Path -LiteralPath $manifestPath) {
    $dictionary = Join-Path $dest 'dictionary'
    if (Test-Path -LiteralPath $dictionary) { Copy-Item -LiteralPath $dictionary -Destination $backup -Recurse }
}
$profile = Join-Path $env:APPDATA 'DAZ 3D\Studio6'
foreach ($name in @('actions.dsx','customactions.dsx','layout.dse','layout.dsx','menus.dsx','toolbars.dsx','StartupScript.dsa')) {
    $file = Join-Path $profile $name
    if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $backup }
}
New-Item -ItemType Directory -Path $dest -Force | Out-Null
foreach ($name in @('runtime','dictionary','source','help')) {
    $target = Join-Path $dest $name
    if (-not (Test-Path -LiteralPath $target)) { New-Item -ItemType Directory -Path $target | Out-Null }
    Get-ChildItem -LiteralPath (Join-Path $source $name) -Force | ForEach-Object {
        if ($name -eq 'dictionary' -and $_.Name -eq 'overrides.json' -and (Test-Path -LiteralPath (Join-Path $target $_.Name))) { return }
        Copy-Item -LiteralPath $_.FullName -Destination $target -Recurse -Force
    }
}
foreach ($name in @('Start.ps1','Uninstall.ps1','Check-Compatibility.ps1','build-info.json','README-简体中文.md','LICENSE.txt','THIRD-PARTY-NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination $dest -Force
}
foreach ($name in @('启动中文版.cmd','启动英文版.cmd','卸载汉化.cmd','检查兼容性.cmd')) { Copy-Item -LiteralPath (Join-Path $source $name) -Destination $dest -Force }
New-Item -ItemType Directory -Path (Join-Path $dest 'reports') -Force | Out-Null
foreach ($name in @('验收结果.md','具体漏翻位置.csv','官方接口来源.md','crash-analysis.json','message-acceptance.json','acceptance-workflow.json','coverage-runtime.json','paint-adapter.json')) {
    $sourceFile = Join-Path $source ('reports\'+$name)
    if (Test-Path -LiteralPath $sourceFile) { Copy-Item -LiteralPath $sourceFile -Destination (Join-Path $dest 'reports') -Force }
}
$screenshotSource = Join-Path $source 'reports\screenshots'
if (Test-Path -LiteralPath $screenshotSource) {
    $screenshotTarget = Join-Path $dest 'reports\screenshots'
    New-Item -ItemType Directory -Path $screenshotTarget -Force | Out-Null
    Get-ChildItem -LiteralPath $screenshotSource -Filter '*.png' -File | Copy-Item -Destination $screenshotTarget -Force
}
New-Item -ItemType Directory -Path (Join-Path $dest 'tools') -Force | Out-Null
foreach ($name in @('launch_test.py','test_bridge.dsa','control.py','run_acceptance.py','run_message_acceptance.py','exit_monitor.py')) {
    Copy-Item -LiteralPath (Join-Path $source ('tools\'+$name)) -Destination (Join-Path $dest 'tools') -Force
}
$nativeInstalled = $false
if (Test-Path -LiteralPath $DazPluginsPath) {
    try {
        Copy-Item -LiteralPath (Join-Path $dest 'runtime\generic\dazzhcn.dll') -Destination (Join-Path $DazPluginsPath 'dazzhcn.dll') -Force -ErrorAction Stop
        [IO.File]::WriteAllText((Join-Path $DazPluginsPath 'dazzhcn-root.txt'), $dest, (New-Object Text.UTF8Encoding($false)))
        $nativeInstalled = $true
    } catch {
        $nativeInstalled = $false
    }
}
$manifest = @{product='DazStudioZhCN';installed=(Get-Date).ToString('o');destination=$dest;backup=$backup;source=$source;version='1.0.0';nativePluginInstalled=$nativeInstalled;dazPluginsPath=$DazPluginsPath}
if (Test-Path -LiteralPath (Join-Path $source 'SAFETY-DISABLED.json')) { Copy-Item -LiteralPath (Join-Path $source 'SAFETY-DISABLED.json') -Destination $dest -Force } elseif (Test-Path -LiteralPath (Join-Path $dest 'SAFETY-DISABLED.json')) { Remove-Item -LiteralPath (Join-Path $dest 'SAFETY-DISABLED.json') }
$desktop = [Environment]::GetFolderPath('Desktop')
$desktopBackups = @{}
if ($previousManifest -and $previousManifest.desktopBackups) {
    foreach ($property in $previousManifest.desktopBackups.PSObject.Properties) { $desktopBackups[$property.Name] = $property.Value }
}
if (-not $NoDesktop) { foreach ($entry in @(@('Daz Studio 简体中文.cmd',''),@('Daz Studio 英文.cmd',' -English'))) {
    $existing = Join-Path $desktop $entry[0]
    if (Test-Path -LiteralPath $existing) {
        $currentText = [IO.File]::ReadAllText($existing,[Text.Encoding]::GetEncoding(936))
        $ourStart = Join-Path $dest 'Start.ps1'
        if (-not $currentText.Contains($ourStart)) { $saved = Join-Path $backup $entry[0]; Copy-Item -LiteralPath $existing -Destination $saved; $desktopBackups[$entry[0]] = $saved }
    }
    $content = '@echo off' + "`r`n" + 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $dest 'Start.ps1') + '"' + $entry[1] + "`r`n" + 'if errorlevel 1 pause' + "`r`n"
    [IO.File]::WriteAllText((Join-Path $desktop $entry[0]),$content,[Text.Encoding]::GetEncoding(936))
} }
$manifest.desktopBackups = $desktopBackups
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Host ('已安装：' + $dest)
Write-Host ('原设置已备份：' + $backup)
if (-not $NoDesktop) { Write-Host '桌面已放置中文、英文启动命令。' }
