#define MyAppName      "OMERewriter"
#define MyAppVersion   "0.1.0"
#define MyAppPublisher "Matthias Klumpp"
#define MyAppURL       "https://github.com/bothlab/omerewriter"
#define MyAppExeName   "OMERewriter.exe"
; Relative path to the CMake install tree produced by the CI (install/OMERewriter/)
#define DeployDir      "..\install\OMERewriter"

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
OutputBaseFilename={#MyAppName}-Setup-{#MyAppVersion}
SetupIconFile={#DeployDir}\share\icons\omerewriter.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

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
Source: "{#DeployDir}\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt runtime, OME libraries, plugins, and everything else windeployqt placed in bin/
Source: "{#DeployDir}\bin\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "{#MyAppExeName}"
Source: "{#DeployDir}\share\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

; Licence / readme that the CI copies to the install root
Source: "{#DeployDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DeployDir}\README.md";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";         Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[UninstallDelete]
; Remove any files the app writes next to itself at runtime
Type: filesandordirs; Name: "{app}\*.log"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
