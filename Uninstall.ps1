param([string]$BackupDirectory)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($PSScriptRoot)
$file = Join-Path $root 'installation.json'
if (-not (Test-Path -LiteralPath $file)) { Write-Host '这是源码/便携目录。关闭 Daz 后删除此目录即可移除该便携副本。'; exit 0 }
$m = Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
if ($m.product -ne 'DazStudioZhCN' -or $root.TrimEnd('\') -ne ([IO.Path]::GetFullPath($m.destination)).TrimEnd('\')) { throw '安装记录与目录不符，已停止卸载。' }
if (Get-Process -Name DAZStudio -ErrorAction SilentlyContinue) { throw '请先正常关闭 Daz Studio，再运行卸载。' }
$backup = if ($BackupDirectory) { [IO.Path]::GetFullPath($BackupDirectory) } else { Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('DazStudioZhCN词库备份\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if ($backup.StartsWith($root.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw '词库备份位置应在安装目录以外。' }
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'dictionary') -Destination $backup -Recurse
$desktop = [Environment]::GetFolderPath('Desktop')
foreach ($name in @('Daz Studio 简体中文.cmd','Daz Studio 英文.cmd')) {
    $p = Join-Path $desktop $name
    if (Test-Path -LiteralPath $p) {
        $content = [IO.File]::ReadAllText($p,[Text.Encoding]::GetEncoding(936))
        if ($content.Contains((Join-Path $root 'Start.ps1'))) {
            $previous = $m.desktopBackups.PSObject.Properties[$name]
            if ($previous -and (Test-Path -LiteralPath ([string]$previous.Value))) { Copy-Item -LiteralPath ([string]$previous.Value) -Destination $p -Force } else { Remove-Item -LiteralPath $p }
        }
    }
}
$pluginsDir = if ($m.dazPluginsPath) { [string]$m.dazPluginsPath } else { 'C:\Program Files\DAZ 3D\DAZStudio6\plugins' }
foreach ($pFile in @('dazzhcn.dll','dazzhcn-root.txt')) {
    $targetPlugin = Join-Path $pluginsDir $pFile
    if (Test-Path -LiteralPath $targetPlugin) {
        try { Remove-Item -LiteralPath $targetPlugin -Force -ErrorAction Stop } catch {}
    }
}
if ([IO.Path]::GetPathRoot($root).TrimEnd('\') -eq $root.TrimEnd('\')) { throw '卸载目标不可为磁盘根目录。' }
Set-Location -LiteralPath $env:TEMP
Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $root) {
    Start-Process -FilePath 'cmd.exe' -ArgumentList ('/c ping 127.0.0.1 -n 2 >nul & rmdir /s /q "' + $root + '"') -WindowStyle Hidden
}
Write-Host ('汉化已卸载。自定义词库保留在：' + $backup)
