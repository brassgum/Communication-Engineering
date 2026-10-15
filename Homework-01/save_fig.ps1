$csvFiles = Get-ChildItem -Path "./out" -Filter "*.csv"

foreach ($file in $csvFiles) {
    python save_fig.py $file.FullName
}
