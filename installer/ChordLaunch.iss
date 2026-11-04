; ChordLaunch インストーラー (Inno Setup 6)
; ユーザー権限でインストール可能 (管理者権限不要)。
; ビルドは scripts\Build-Release.ps1 から実行する。単体実行時は /DAppVersion=v26.11-Pre のように指定可能。

#ifndef AppVersion
  #define AppVersion "v26.11-Pre"
#endif
#ifndef AppFileVersion
  #define AppFileVersion "26.11.0.0"
#endif

#define AppName "ChordLaunch"
#define AppPublisher "stellorbit"
#define AppExeName "ChordLaunch.exe"
#define AppUrl "https://github.com/stellorbit/ChordLaunch"

[Setup]
AppId={{6F0B2E4A-3C1D-4B7A-9E52-8D4A1C7F5B30}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
AppUpdatesURL={#AppUrl}/releases
VersionInfoVersion={#AppFileVersion}
VersionInfoProductVersion={#AppFileVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=ChordLaunch-{#AppVersion}-setup
SetupIconFile=..\resources\ChordLaunch.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
