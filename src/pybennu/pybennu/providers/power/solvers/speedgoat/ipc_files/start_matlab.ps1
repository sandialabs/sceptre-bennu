# start_matlab.ps1
param (
    [string]$MatlabPath = "F:\MATLAB\bin\matlab.exe",
    [string]$ListenerScript = "C:\MatlabIPC\speedgoat_listener.m",
    [string]$LogFile = "matlab.log",
    [string]$ErrFile = "matlab-err.log"
)

$argumentList = @(
    "-nosplash",
    "-nodesktop",
    "-r",
    "run('$ListenerScript')"
)

Start-Process -FilePath $MatlabPath `
    -ArgumentList $argumentList `
    -RedirectStandardOutput $LogFile `
    -RedirectStandardError $ErrFile `
    -NoNewWindow
