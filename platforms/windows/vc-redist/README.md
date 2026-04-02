# Documentation Microsoft Visual C++ Redistributable
## Télécharger la dernière version des redistribuable Visual Studio (x86 / x64 / arm64) :
https://learn.microsoft.com/fr-fr/cpp/windows/latest-supported-vc-redist?view=msvc-170

<br /><br />

---

<br /><br />

## Version des redistribuable versionner dans ce repository : v14.50.35719

<br /><br />

---

<br /><br />

## Commande PowerShell pour checker la présence des fichiers des vc_redist installer précédemment (x64/arm64/x86) : 
**INFORMATION** : Les fichiers .dll, finissant par `d` sont SEULEMENT pour les builds en mode `Debug`.
```powershell
Get-ChildItem "$env:WINDIR\System32" -Filter "msvcp140*.dll"
Get-ChildItem "$env:WINDIR\System32" -Filter "vcruntime140*.dll"
Get-ChildItem "$env:WINDIR\System32" -Filter "concrt140*.dll"
Get-ChildItem "$env:WINDIR\System32" -Filter "vccorlib140*.dll"


# Résultat attendu :
PS C:\Users\Corentin> Get-ChildItem "$env:WINDIR\System32" -Filter "msvcp140*.dll"


    Répertoire : C:\WINDOWS\System32


Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
-a----        21/11/2025     22:56         553552 msvcp140.dll
-a----        11/06/2025     05:21         922752 msvcp140d.dll
-a----        11/06/2025     05:21          94848 msvcp140d_atomic_wait.dll
-a----        11/06/2025     05:21          35968 msvcp140d_codecvt_ids.dll
-a----        21/11/2025     22:56          35488 msvcp140_1.dll
-a----        11/06/2025     05:21          41600 msvcp140_1d.dll
-a----        21/11/2025     22:56         278608 msvcp140_2.dll
-a----        11/06/2025     05:21         464512 msvcp140_2d.dll
-a----        21/11/2025     22:56          48800 msvcp140_atomic_wait.dll
-a----        24/06/2022     23:31         571280 msvcp140_clr0400.dll
-a----        21/11/2025     22:56          31392 msvcp140_codecvt_ids.dll


PS C:\Users\Corentin> Get-ChildItem "$env:WINDIR\System32" -Filter "vcruntime140*.dll"


    Répertoire : C:\WINDOWS\System32


Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
-a----        21/11/2025     22:56         123472 vcruntime140.dll
-a----        11/06/2025     05:21         196224 vcruntime140d.dll
-a----        21/11/2025     22:56          47264 vcruntime140_1.dll
-a----        11/06/2025     05:21          65136 vcruntime140_1d.dll
-a----        24/06/2022     23:31          37800 vcruntime140_1_clr0400.dll
-a----        24/06/2022     23:31          98728 vcruntime140_clr0400.dll
-a----        21/11/2025     22:56          37456 vcruntime140_threads.dll
-a----        11/06/2025     05:21          51360 vcruntime140_threadsd.dll


PS C:\Users\Corentin> Get-ChildItem "$env:WINDIR\System32" -Filter "concrt140*.dll"


    Répertoire : C:\WINDOWS\System32


Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
-a----        21/11/2025     22:56         321696 concrt140.dll
-a----        11/06/2025     05:21         726144 concrt140d.dll


PS C:\Users\Corentin> Get-ChildItem "$env:WINDIR\System32" -Filter "vccorlib140*.dll"


    Répertoire : C:\WINDOWS\System32


Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
-a----        21/11/2025     22:56         350880 vccorlib140.dll
-a----        11/06/2025     05:21        1490592 vccorlib140d.dll
```

<br /><br />

---

<br /><br />

## Commande PowerShell pour checker la version des .dll des vc_redist installer précédemment (x64/arm64/x86)
```powershell
Get-ChildItem "$env:WINDIR\System32\*.dll" -File |
Where-Object {
  $_.Name -match '^(msvcp140|vcruntime140|concrt140|vccorlib140).*\.dll$' -and
  $_.Name -notmatch 'd\.dll$' -and
  $_.Name -notmatch '_clr0400\.dll$'
} |
Select-Object Name, @{Name="Version";Expression={$_.VersionInfo.FileVersion}}, LastWriteTime |
Sort-Object Name


# Résultat attendu :
Name                      Version       LastWriteTime
----                      -------       -------------
concrt140.dll             14.50.35719.0 21/11/2025 22:56:28
msvcp140.dll              14.50.35719.0 21/11/2025 22:56:30
msvcp140_1.dll            14.50.35719.0 21/11/2025 22:56:30
msvcp140_2.dll            14.50.35719.0 21/11/2025 22:56:30
msvcp140_atomic_wait.dll  14.50.35719.0 21/11/2025 22:56:32
msvcp140_codecvt_ids.dll  14.50.35719.0 21/11/2025 22:56:30
msvcp140d_atomic_wait.dll 14.44.35211.0 11/06/2025 05:21:58
msvcp140d_codecvt_ids.dll 14.44.35211.0 11/06/2025 05:21:56
vccorlib140.dll           14.50.35719.0 21/11/2025 22:56:32
vcruntime140.dll          14.50.35719.0 21/11/2025 22:56:32
vcruntime140_1.dll        14.50.35719.0 21/11/2025 22:56:32
vcruntime140_threads.dll  14.50.35719.0 21/11/2025 22:56:32
```

<br /><br />

---

<br /><br />

## ✅ Commande PowerShell pour copier les DLL (x64/arm64/x86) Release vers ce repository :
```powershell
# Changer le PATH si besoin
# Pour Windows arm64 via Parellel Desktop macOS, il vaut mieux pas utilisé un dossier partagé avec le macOS il vaut mieux mettre par exemple à : C:\Temp\
# puis les copier ensuite dans le repository.
$dest = "C:\Users\Corentin\Desktop\OrganizationCrzGames\Crzgames_RC2D_GameTemplate\platforms\windows\vc-redist\x64-release"

New-Item -ItemType Directory -Force -Path $dest | Out-Null

Get-ChildItem "$env:WINDIR\System32\*.dll" -File |
Where-Object {
    # garder uniquement les familles
    ($_.Name.StartsWith("msvcp140") -or $_.Name.StartsWith("vcruntime140") -or $_.Name.StartsWith("concrt140") -or $_.Name.StartsWith("vccorlib140")) -and
    # exclure debug : noms qui contiennent un 'd' juste après 140 (ex: msvcp140d_...) ou finissent en ...d.dll (ex: msvcp140_1d.dll)
    (-not $_.Name.Contains("140d")) -and
    (-not $_.Name.EndsWith("d.dll")) -and
    # exclure clr0400
    (-not $_.Name.EndsWith("_clr0400.dll"))
} |
Copy-Item -Destination $dest -Force

Write-Host "Copie Release vc-redist terminée dans $dest"
```