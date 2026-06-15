# Setup devkitPro toolchain for 3DS AA plugin
# Downloads MSYS2 and installs devkitARM + libctru

$ErrorActionPreference = "Stop"
$MSYS2_ROOT = "C:\msys64"
$MSYS2_INSTALLER = "$env:TEMP\msys2-installer.exe"
$DOWNLOAD_URL = "https://repo.msys2.org/distrib/x86_64/msys2-x86_64-latest.exe"
$DEVKITPRO_PACMAN_CONF = "https://pkg.devkitpro.org/devkitpro-pacman.conf"

Write-Host "=== Step 1: Checking existing MSYS2 ==="
if (Test-Path "$MSYS2_ROOT\usr\bin\pacman.exe") {
    Write-Host "MSYS2 already installed at $MSYS2_ROOT"
} else {
    Write-Host "=== Step 1: Downloading MSYS2 installer (~100MB) ==="
    $webClient = New-Object System.Net.WebClient
    $webClient.DownloadFile($DOWNLOAD_URL, $MSYS2_INSTALLER)
    Write-Host "Downloaded to $MSYS2_INSTALLER"

    Write-Host "=== Step 2: Installing MSYS2 to $MSYS2_ROOT ==="
    $proc = Start-Process -FilePath $MSYS2_INSTALLER -ArgumentList "install","--default-answer","--confirm-command","--root","$MSYS2_ROOT" -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) {
        Write-Host "MSYS2 install failed with exit code: $($proc.ExitCode)"
        exit 1
    }
    Write-Host "MSYS2 installed successfully"
}

Write-Host "=== Step 3: Configuring devkitPro pacman repository ==="
$pacman_conf = "$MSYS2_ROOT\etc\pacman.conf"
$repo_entry = @"

[devkitpro]
SigLevel = Never
Server = https://pkg.devkitpro.org
"@

if (-not (Select-String -Path $pacman_conf -Pattern "devkitpro" -SimpleMatch -Quiet)) {
    Add-Content -Path $pacman_conf -Value $repo_entry
    Write-Host "Added devkitpro repo to pacman.conf"
} else {
    Write-Host "devkitpro repo already in pacman.conf"
}

Write-Host "=== Step 4: Initializing pacman keys ==="
$env:MSYSTEM = "MSYS"
& "$MSYS2_ROOT\usr\bin\bash.exe" -l -c "pacman-key --init 2>&1" 2>&1 | Write-Host
& "$MSYS2_ROOT\usr\bin\bash.exe" -l -c "pacman-key --populate 2>&1" 2>&1 | Write-Host

Write-Host "=== Step 5: Updating package databases ==="
& "$MSYS2_ROOT\usr\bin\bash.exe" -l -c "pacman -Syu --noconfirm 2>&1" 2>&1 | Write-Host

Write-Host "=== Step 6: Installing devkitARM + libctru + 3DS tools ==="
& "$MSYS2_ROOT\usr\bin\bash.exe" -l -c "pacman -S --noconfirm --needed devkitARM 3ds-dev 3dslink 2>&1" 2>&1 | Write-Host

Write-Host ""
Write-Host "=== DONE ==="
Write-Host "devkitPro toolchain installed!"
Write-Host "To build, run in MSYS2 shell:"
Write-Host "  cd /l/Code/3DS_AA && make"
