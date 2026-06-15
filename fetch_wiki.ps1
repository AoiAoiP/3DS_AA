# Fetch devkitPro pacman wiki page
try {
    Add-Type -AssemblyName System.Web
    $wc = New-Object System.Net.WebClient
    $wc.Headers.Add('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)')
    $content = $wc.DownloadString('https://devkitpro.org/wiki/devkitPro_pacman')
    # Strip HTML tags for readability
    $clean = $content -replace '<[^>]+>', ' '
    $clean = $clean -replace '\s+', ' '
    $clean = $clean -replace '&nbsp;', ' '
    $clean = $clean -replace '&amp;', '&'
    $clean = $clean -replace '&lt;', '<'
    $clean = $clean -replace '&gt;', '>'
    $clean = $clean -replace '&quot;', '"'
    $lines = $clean -split '\. '
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -gt 5) {
            Write-Host $trimmed
            Write-Host "---"
        }
    }
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
