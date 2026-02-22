#define AppName      "OMERewriter"
#define AppPublisher "Matthias Klumpp"
#define AppURL       "https://github.com/bothlab/omerewriter"
#define AppExeName   "OMERewriter.exe"
; These can be overridden from the command line:
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef AppVersionNumeric
  #define AppVersionNumeric "0.1.0"
#endif
#ifndef DeployDir
  #define DeployDir "..\install\OMERewriter"
#endif

[Setup]
AppId={{BBF09373-0A38-451C-83E3-AE5673BB13A1}}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

; Installer exe metadata
VersionInfoVersion={#AppVersionNumeric}
VersionInfoDescription={#AppName} Installer
VersionInfoCompany={#AppPublisher}
VersionInfoCopyright=Copyright (C) 2025-2026 {#AppPublisher}

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; Ask for admin rights only when installing system-wide; allows per-user installs too
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

LicenseFile={#DeployDir}\LICENSE.txt

OutputDir=output
OutputBaseFilename={#AppName}-{#AppVersion}_Setup
SetupIconFile={#DeployDir}\share\icons\omerewriter.ico
UninstallDisplayIcon={app}\bin\{#AppExeName}

Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
; Require Windows 10 or later (Qt 6 minimum)
MinVersion=10.0
; Offer to close any running instance before installing/uninstalling
CloseApplications=yes
CloseApplicationsFilter={#AppExeName}
RestartApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[InstallDelete]
; Remove the entire deployed tree before installing so that upgrading
; over an existing version leaves no stale files from the previous release.
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\lib"
Type: filesandordirs; Name: "{app}\libexec"
Type: filesandordirs; Name: "{app}\share"

[Files]
Source: "{#DeployDir}\bin\{#AppExeName}"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#DeployDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "bin\{#AppExeName},LICENSE.txt"
Source: "{#DeployDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}";  Filename: "{app}\bin\{#AppExeName}"
Name: "{autodesktop}\{#AppName}";   Filename: "{app}\bin\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
