[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Get-EngineRoot {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Association
	)

	$candidates = [System.Collections.Generic.List[string]]::new()

	if ([System.IO.Path]::IsPathRooted($Association)) {
		$candidates.Add($Association)
	}

	$userBuilds = Get-ItemProperty -LiteralPath 'Registry::HKEY_CURRENT_USER\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
	if ($null -ne $userBuilds) {
		$registeredBuild = $userBuilds.PSObject.Properties[$Association]
		if ($null -ne $registeredBuild -and -not [string]::IsNullOrWhiteSpace([string]$registeredBuild.Value)) {
			$candidates.Add([string]$registeredBuild.Value)
		}
	}

	$launcherRegistryPaths = @(
		"Registry::HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\$Association",
		"Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$Association"
	)

	foreach ($registryPath in $launcherRegistryPaths) {
		$engineRegistration = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
		if ($null -ne $engineRegistration -and -not [string]::IsNullOrWhiteSpace([string]$engineRegistration.InstallLocation)) {
			$candidates.Add([string]$engineRegistration.InstallLocation)
		}
	}

	$programFiles = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)
	if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
		$candidates.Add((Join-Path $programFiles "Epic Games\UE_$Association"))
	}

	foreach ($candidate in ($candidates | Select-Object -Unique)) {
		$buildBatch = Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'
		if (Test-Path -LiteralPath $buildBatch -PathType Leaf) {
			return [System.IO.Path]::GetFullPath($candidate)
		}
	}

	throw "Unreal Engine '$Association' installation containing Engine\Build\BatchFiles\Build.bat was not found."
}

try {
	$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
	$projectFiles = @(Get-ChildItem -LiteralPath $projectRoot -File -Filter '*.uproject')

	if ($projectFiles.Count -ne 1) {
		throw "Expected exactly one .uproject in '$projectRoot', but found $($projectFiles.Count)."
	}

	$projectFile = $projectFiles[0]
	$projectDescriptor = Get-Content -Raw -LiteralPath $projectFile.FullName | ConvertFrom-Json
	$engineAssociation = [string]$projectDescriptor.EngineAssociation

	if ([string]::IsNullOrWhiteSpace($engineAssociation)) {
		throw "EngineAssociation is missing from '$($projectFile.FullName)'."
	}

	$sourceDirectory = Join-Path $projectRoot 'Source'
	$editorTargetFiles = @(Get-ChildItem -LiteralPath $sourceDirectory -Recurse -File -Filter '*Editor.Target.cs')

	if ($editorTargetFiles.Count -ne 1) {
		throw "Expected exactly one Editor Target under '$sourceDirectory', but found $($editorTargetFiles.Count)."
	}

	$editorTarget = $editorTargetFiles[0].Name -replace '\.Target\.cs$', ''
	$engineRoot = Get-EngineRoot -Association $engineAssociation
	$buildBatch = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
	$buildArguments = @(
		$editorTarget,
		'Win64',
		'Development',
		"-Project=$($projectFile.FullName)",
		'-WaitMutex',
		'-FromMsBuild'
	)

	$displayArguments = $buildArguments | ForEach-Object {
		if ($_ -match '\s') {
			'"{0}"' -f ($_ -replace '"', '\"')
		}
		else {
			$_
		}
	}

	Write-Host "Project: $($projectFile.FullName)"
	Write-Host "Editor Target: $editorTarget"
	Write-Host "Engine Association: $engineAssociation"
	Write-Host "Engine Root: $engineRoot"
	Write-Host ('Build command: "{0}" {1}' -f $buildBatch, ($displayArguments -join ' '))

	& $buildBatch @buildArguments
	$buildExitCode = $LASTEXITCODE

	if ($null -eq $buildExitCode) {
		throw 'Build.bat did not return an exit code.'
	}

	if ($buildExitCode -ne 0) {
		Write-Error "Editor build failed with exit code $buildExitCode."
		exit $buildExitCode
	}

	Write-Host 'Editor build succeeded.'
	exit 0
}
catch {
	Write-Error $_
	exit 1
}
