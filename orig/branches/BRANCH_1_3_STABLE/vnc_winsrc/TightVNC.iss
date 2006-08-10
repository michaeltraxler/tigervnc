; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
AppName=TightVNC
AppVerName=TightVNC 1.3.7
AppVersion=1.3.7
AppPublisher=Constantin Kaplinsky
AppPublisherURL=http://www.tightvnc.com
AppSupportURL=http://www.tightvnc.com
AppUpdatesURL=http://www.tightvnc.com
DefaultDirName={pf}\TightVNC
DefaultGroupName=TightVNC
AllowNoIcons=yes
InfoBeforeFile=InstInfo.rtf
Compression=zip/9
WindowVisible=no
DisableStartupPrompt=yes
BackColor=clBlack
BackColor2=clBlue

ChangesAssociations=yes

[Components]
Name: "server"; Description: "TightVNC Server"; Types: full compact custom;
Name: "viewer"; Description: "TightVNC Viewer"; Types: full compact custom;
Name: "webdoc"; Description: "Web pages and documentation"; Types: full custom;

[Files]
Source: "WinVNC.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace; Components: server
Source: "VNCHooks.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace; Components: server
Source: "vncviewer.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: viewer
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENCE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "TightVNC.url"; DestDir: "{app}"; Flags: ignoreversion
Source: "TightVNC-donate.url"; DestDir: "{app}"; Flags: ignoreversion
Source: "Web\*"; DestDir: "{app}\Web"; Flags: ignoreversion; Components: webdoc
Source: "Web\images\*"; DestDir: "{app}\Web\images"; Flags: ignoreversion; Components: webdoc
Source: "Web\logo\*"; DestDir: "{app}\Web\logo"; Flags: ignoreversion; Components: webdoc
Source: "Web\doc\win32\*"; DestDir: "{app}\Web\doc\win32"; Flags: ignoreversion; Components: webdoc
Source: "Web\doc\java\*"; DestDir: "{app}\Web\doc\java"; Flags: ignoreversion; Components: webdoc
Source: "Web\doc\man\*"; DestDir: "{app}\Web\doc\man"; Flags: ignoreversion; Components: webdoc
Source: "Web\doc\unix\*"; DestDir: "{app}\Web\doc\unix"; Flags: ignoreversion; Components: webdoc

[Icons]
Name: "{group}\Launch TightVNC Server";               FileName: "{app}\WinVNC.exe";                                    WorkingDir: "{app}";     Components: server
Name: "{group}\Show User Settings";                   FileName: "{app}\WinVNC.exe";    Parameters: "-settings";        WorkingDir: "{app}";     Components: server
Name: "{group}\TightVNC Viewer";                      FileName: "{app}\vncviewer.exe";                                 WorkingDir: "{app}";     Components: viewer
Name: "{group}\Administration\Install VNC Service";   FileName: "{app}\WinVNC.exe";    Parameters: "-install";         WorkingDir: "{app}";     Components: server
Name: "{group}\Administration\Remove VNC Service";    FileName: "{app}\WinVNC.exe";    Parameters: "-remove";          WorkingDir: "{app}";     Components: server
Name: "{group}\Administration\Run Service Helper";    FileName: "{app}\WinVNC.exe";    Parameters: "-servicehelper";   WorkingDir: "{app}";     Components: server
Name: "{group}\Administration\Show Default Settings"; FileName: "{app}\WinVNC.exe";    Parameters: "-defaultsettings"; WorkingDir: "{app}";     Components: server
Name: "{group}\Documentation\About VNC and TightVNC"; FileName: "{app}\Web\index.html";                                WorkingDir: "{app}\Web"; Components: webdoc
Name: "{group}\Documentation\Installation and Getting Started"; FileName: "{app}\Web\winst.html";                      WorkingDir: "{app}\Web"; Components: webdoc
Name: "{group}\Documentation\Licensing Terms";        FileName: "{app}\LICENCE.txt";                                   WorkingDir: "{app}"
Name: "{group}\Documentation\Make a Donation";        FileName: "{app}\TightVNC-donate.url"
Name: "{group}\Documentation\TightVNC Web Site";      FileName: "{app}\TightVNC.url"
Name: "{group}\Documentation\What's New (Detailed Log)"; FileName: "{app}\Web\changelog-win32.html";                   WorkingDir: "{app}\Web"; Components: webdoc
Name: "{group}\Documentation\What's New (Summary)";   FileName: "{app}\Web\whatsnew-devel.html";                       WorkingDir: "{app}\Web"; Components: webdoc

[Tasks]
Name: associate; Description: "&Associate .vnc files with TightVNC Viewer"; GroupDescription: "File associations:"; Components: viewer
Name: installservice; Description: "&Register new TightVNC Server as a system service"; GroupDescription: "Server configuration:"; Components: server; Flags: unchecked
Name: startservice; Description: "&Start or restart TightVNC service"; GroupDescription: "Server configuration:"; Components: server; Flags: unchecked; MinVersion: 0,1

[Registry]
Root: HKCR; Subkey: ".vnc"; ValueType: string; ValueName: ""; ValueData: "VncViewer.Config"; Flags: uninsdeletevalue; Tasks: associate
Root: HKCR; Subkey: "VncViewer.Config"; ValueType: string; ValueName: ""; ValueData: "VNCviewer Config File"; Flags: uninsdeletekey; Tasks: associate
Root: HKCR; Subkey: "VncViewer.Config\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\vncviewer.exe,0"; Tasks: associate
Root: HKCR; Subkey: "VncViewer.Config\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\vncviewer.exe"" -config ""%1"""; Tasks: associate

[Run]
Filename: "{app}\WinVNC.exe"; Parameters: "-reinstall"; Tasks: installservice
Filename: "net"; Parameters: "start WinVNC"; Tasks: startservice
Filename: "{app}\WinVNC.exe"; Parameters: "-servicehelper"; Tasks: startservice

