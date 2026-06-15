# Final attempt - WebClient with all browser headers
$wc = New-Object System.Net.WebClient
$wc.Encoding = [System.Text.Encoding]::UTF8
$wc.Headers.Clear()
$wc.Headers.Add('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36')
$wc.Headers.Add('Accept', 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8')
$wc.Headers.Add('Accept-Language', 'zh-CN,zh;q=0.9,en;q=0.8')
$wc.Headers.Add('Referer', 'https://www.google.com/')

try {
    $html = $wc.DownloadString('https://devkitpro.org/wiki/devkitPro_pacman')
    $html | Out-File -FilePath "L:\Code\3DS_AA\wiki_page.html" -Encoding UTF8
    Write-Host "SUCCESS - Downloaded $($html.Length) chars"
    Write-Host "Saved to L:\Code\3DS_AA\wiki_page.html"
} catch {
    Write-Host "FAILED: $($_.Exception.Message)"

    # Try alternative URLs
    $altUrls = @(
        'https://devkitpro.org/wiki/Getting_Started',
        'https://devkitpro.org/viewtopic.php?f=13&t=8702'
    )
    foreach ($alt in $altUrls) {
        try {
            Write-Host "Trying: $alt"
            $html2 = $wc.DownloadString($alt)
            Write-Host "Got $($html2.Length) chars from $alt"
        } catch {
            Write-Host "  Also failed"
        }
    }
}
