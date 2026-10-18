$lines = Get-Content 'e:\stm32Demos\ModbusDAS\DRIVER\DS3231\ds3231.c'
for ($i = 30; $i -le 34; $i++) {
    Write-Output ('Line ' + ($i+1) + ': ' + $lines[$i])
    ($lines[$i].ToCharArray() | ForEach-Object { '{0} U+{1:X4}' -f $_, [int]$_ }) -join '  '
    Write-Output '---'
}
