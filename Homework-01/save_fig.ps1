$csvFiles = Get-ChildItem -Path "./out" -Filter "*.csv"

foreach ($file in $csvFiles) {
    python visualize.py $file.FullName
}