$python = "C:\Users\15099\AppData\Local\Programs\Python\Python313"

$env:PATH = "$python;$env:PATH"
$env:LUA_CPATH = "$PSScriptRoot\build\?.dll;;"

lua $args