# Fetch devkitPro pacman wiki - with full browser headers
try {
    $url = 'https://devkitpro.org/wiki/devkitPro_pacman'

    # Use HttpClient with proper browser headers
    $handler = New-Object System.Net.Http.HttpClientHandler
    $handler.AutomaticDecompression = [System.Net.DecompressionMethods]::All

    $client = New-Object System.Net.Http.HttpClient
    $client.DefaultRequestHeaders.Add('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36')
    $client.DefaultRequestHeaders.Add('Accept', 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8')
    $client.DefaultRequestHeaders.Add('Accept-Language', 'zh-CN,zh;q=0.9,en;q=0.8')
    $client.DefaultRequestHeaders.Add('Accept-Encoding', 'gzip, deflate, br')
    $client.DefaultRequestHeaders.Add('Referer', 'https://www.google.com/')
    $client.DefaultRequestHeaders.Add('Connection', 'keep-alive')
    $client.DefaultRequestHeaders.Add('Upgrade-Insecure-Requests', '1')
    $client.DefaultRequestHeaders.Add('Sec-Fetch-Dest', 'document')
    $client.DefaultRequestHeaders.Add('Sec-Fetch-Mode', 'navigate')
    $client.DefaultRequestHeaders.Add('Sec-Fetch-Site', 'cross-site')

    $response = $client.GetAsync($url).Result
    $content = $response.Content.ReadAsStringAsync().Result

    # Write raw HTML to file first
    $content | Out-File -FilePath "L:\Code\3DS_AA\wiki_page.html" -Encoding UTF8

    Write-Host "Page downloaded successfully. Length: $($content.Length) chars"
    Write-Host ""
    Write-Host "=== KEY SECTIONS ==="

    # Extract text between known markers, remove HTML tags
    $text = $content -replace '<script[^>]*>.*?</script>', '' -replace '<style[^>]*>.*?</style>', ''
    $text = $text -replace '<[^>]+>', "`n"
    $text = $text -replace '&nbsp;', ' '
    $text = $text -replace '&amp;', '&'
    $text = $text -replace '&lt;', '<'
    $text = $text -replace '&gt;', '>'
    $text = $text -replace '&quot;', '"'
    $text = $text -replace '&#39;', "'"

    # Remove excessive blank lines
    $lines = $text -split "`n" | Where-Object { $_.Trim() -ne '' }
    foreach ($line in $lines) {
        $t = $line.Trim()
        if ($t.Length -gt 3) {
            Write-Host $t
        }
    }

} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
