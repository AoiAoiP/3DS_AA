# Find correct MSYS2 installer URL and download it

$ErrorActionPreference = "Continue"
$MSYS2_ROOT = "C:\msys64"

Write-Host "=== Searching for MSYS2 installer ==="

# Try multiple known URL patterns for MSYS2 installer
$urls = @(
    "https://repo.msys2.org/distrib/x86_64/msys2-x86_64-latest.exe",
    "https://repo.msys2.org/distrib/x86_64/msys2-x86_64-20250501.exe",
    "https://repo.msys2.org/distrib/x86_64/msys2-x86_64-2025.exe",
    "https://repo.msys2.org/distrib/msys2-x86_64-latest.exe",
    "https://github.com/msys2/msys2-installer/releases/latest/download/msys2-x86_64-latest.exe"
)

$webClient = New-Object System.Net.WebClient
$webClient.Headers.Add("User-Agent", "Mozilla/5.0")

$downloaded = $false
foreach ($url in $urls) {
    try {
        Write-Host "Trying: $url"
        # First check if URL exists
        $req = [System.Net.WebRequest]::Create($url)
        $req.Method = "HEAD"
        $req.Timeout = 10000
        $resp = $req.GetResponse()
        $size = $resp.ContentLength
        $resp.Close()
        Write-Host "  File exists! Size: $size bytes"

        $output = "$env:TEMP\msys2-installer.exe"
        $webClient.DownloadFile($url, $output)
        Write-Host "  Downloaded to: $output"
        $downloaded = $true
        break
    } catch {
        Write-Host "  Failed: $_"
    }
}

if (-not $downloaded) {
    Write-Host "All URLs failed. Trying to list directory..."
    try {
        $html = (New-Object System.Net.WebClient).DownloadString("https://repo.msys2.org/distrib/x86_64/")
        $matches = [regex]::Matches($html, 'href="([^"]+\.exe)"')
        foreach ($m in $matches) {
            Write-Host "Found: $($m.Groups[1].Value)"
        }
    } catch {
        Write-Host "Cannot list directory: $_"
    }
}
