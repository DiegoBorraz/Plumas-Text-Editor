#define MyAppName "Plumas Text Editor"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Diego Avila"
#define MyAppURL "https://github.com/DiegoBorraz/Plumas-Text-Editor"
#define MyAppExeName "plumas-text-editor.exe"

[Setup]
AppId={{A4B8F2E1-7C3D-4F9A-9B2E-1D5C6A7E8F90}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\Plumas Text Editor
DefaultGroupName=Plumas Text Editor
OutputDir=dist
OutputBaseFilename=PlumasTextEditor-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
LicenseFile=..\..\packaging\debian\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "staging\plumas-text-editor.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "staging\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "staging\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "staging\share\*"; DestDir: "{app}\share"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "staging\lib\*"; DestDir: "{app}\lib"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\share"
Type: filesandordirs; Name: "{app}\lib"
