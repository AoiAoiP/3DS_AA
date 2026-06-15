# Download devkitPro installer
Write-Host "=== Checking latest devkitPro installer ==="

# Try known URLs
$urls = @(
    "https://github.com/devkitPro/installer/releases/download/v3.0.4/devkitProUpdater-3.0.4.exe",
    "https://github.com/devkitPro/installer/releases/download/v3.0.3/devkitProUpdater-3.0.3.exe"
)

$webClient = New-Object System.Net.WebClient
$webClient.Headers.Add("User-Agent", "PowerShell")

$downloaded = $false
foreach ($url in $urls) {
    try {
        Write-Host "Trying: $url"
        $output = "L:\Code\3DS_AA\devkitProUpdater.exe"
        $webClient.DownloadFile($url, $output)
        Write-Host "Downloaded to: $output"
        $downloaded = $true
        break
    } catch {
        Write-Host "Failed: $_"
    }
}

if (-not $downloaded) {
    Write-Host "Could not download installer. Please download manually from:"
    Write-Host "  https://devkitpro.org/wiki/devkitPro_pacman"
    Write-Host "  Or: https://github.com/devkitPro/installer/releases/latest"
    exit 1
}
