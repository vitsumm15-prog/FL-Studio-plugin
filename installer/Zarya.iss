; Inno Setup script: installs the Zarya VST3 into the shared VST3 folder and the standalone app.
#ifndef AppVersion
  #define AppVersion "0.9.1"
#endif
#ifndef ArtefactsDir
  #define ArtefactsDir "..\build\Zarya_artefacts\Release"
#endif

[Setup]
AppId={{08D96974-5380-4B87-84AA-070775A7CB78}
AppName=Zarya
AppVersion={#AppVersion}
AppPublisher=Zarya Audio
DefaultDirName={autopf}\Zarya
DefaultGroupName=Zarya
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=Zarya-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Zarya Synth

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#ArtefactsDir}\VST3\Zarya.vst3\*"; DestDir: "{commoncf64}\VST3\Zarya.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtefactsDir}\Standalone\Zarya.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\Zarya"; Filename: "{app}\Zarya.exe"; Components: standalone
Name: "{autodesktop}\Zarya"; Filename: "{app}\Zarya.exe"; Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone; Flags: unchecked

[Run]
Filename: "{app}\Zarya.exe"; Description: "Launch Zarya"; Components: standalone; Flags: nowait postinstall skipifsilent
