# send_matlab_command.ps1
param (
    [string]$command
)

$commandFile = "C:\MatlabIPC\command.txt"
$resultFile = "C:\MatlabIPC\result.txt"
$commandLockFile = "C:\MatlabIPC\command.lock"
$resultLockFile = "C:\MatlabIPC\result.lock"

function Acquire-Lock {
    param([string]$lockPath, [int]$timeout=30)
    $elapsed = 0
    while ((Test-Path $lockPath) -and ($elapsed -lt $timeout)) {
        Start-Sleep -Milliseconds 200
        $elapsed += 0.2
    }
    if (Test-Path $lockPath) {
        throw "Timeout waiting for lock on $lockPath"
    }
    # Create the lock file atomically
    New-Item -Path $lockPath -ItemType File -Force | Out-Null
}

function Release-Lock {
    param([string]$lockPath)
    if (Test-Path $lockPath) {
        Remove-Item $lockPath -Force
    }
}

function Send-CommandWithID {
    param([string]$cmd)

    # Generate unique ID for this command
    $id = [guid]::NewGuid().ToString()
    $fullCommand = "$id $cmd"

    # Append command with ID to command.txt
    Acquire-Lock -lockPath $commandLockFile
    try {
        Add-Content -Path $commandFile -Value $fullCommand
    }
    finally {
        Release-Lock -lockPath $commandLockFile
    }

    # Wait for matching result line with same ID
    $timeout = 60
    $elapsed = 0
    while ($elapsed -lt $timeout) {
        try {
            Acquire-Lock -lockPath $resultLockFile -timeout 10
        }
        catch {
            # Could not acquire lock within 10 seconds, retry
            Start-Sleep -Milliseconds 200
            $elapsed += 0.2
            continue
        }

        try {
            if (-not (Test-Path $resultFile)) {
                # No results yet, release lock and wait
                Release-Lock -lockPath $resultLockFile
                Start-Sleep -Milliseconds 200
                $elapsed += 0.2
                continue
            }

            $lines = Get-Content -Path $resultFile
            $matchIndex = $null
            for ($i=0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -like "$id*") {
                    $matchIndex = $i
                    break
                }
            }

            if ($matchIndex -ne $null) {
                $matchedLine = $lines[$matchIndex]
                # Remove matched line from results
                if ($lines.Count -gt 1) {
                    $lines = $lines[0..($matchIndex-1)] + $lines[($matchIndex+1)..($lines.Count-1)]
                    $lines | Set-Content -Path $resultFile
                }
                else {
                    Clear-Content -Path $resultFile
                }
                # Extract result (remove ID prefix)
                $result = $matchedLine.Substring($id.Length).Trim()
                return $result
            }
        }
        finally {
            # Always release the lock even if error occurs
            Release-Lock -lockPath $resultLockFile
        }

        Start-Sleep -Milliseconds 200
        $elapsed += 0.2
    }

    throw "Timeout waiting for result with ID $id"
}

try {
    $response = Send-CommandWithID -cmd $command
    Write-Output $response
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
