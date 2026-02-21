#define MyAppName      "OMERewriter"
#define MyAppPublisher "Matthias Klumpp"
#define MyAppURL       "https://github.com/bothlab/omerewriter"
#define MyAppExeName   "OMERewriter.exe"
; These can be overridden from the command line:
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef DeployDir
  #define DeployDir "..\install\OMERewriter"
#endif

[Setup]
AppId={{BBF09373-0A38-451C-83E3-AE5673BB13A1}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Installer exe metadata
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription={#MyAppName} Installer
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright=Copyright (C) 2025-2026 {#MyAppPublisher}

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
; Ask for admin rights only when installing system-wide; allows per-user installs too
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

LicenseFile={#DeployDir}\LICENSE.txt

OutputDir=output
OutputBaseFilename={#MyAppName}-{#MyAppVersion}_Setup
SetupIconFile={#DeployDir}\share\icons\omerewriter.ico
UninstallDisplayIcon={app}\bin\{#MyAppExeName}

Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
; Require Windows 10 or later (Qt 6 minimum)
MinVersion=10.0
; Offer to close any running instance before installing/uninstalling
CloseApplications=yes
CloseApplicationsFilter=*.exe
RestartApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#DeployDir}\bin\{#MyAppExeName}"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#DeployDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "bin\{#MyAppExeName},LICENSE.txt"
Source: "{#DeployDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";         Filename: "{app}\bin\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
