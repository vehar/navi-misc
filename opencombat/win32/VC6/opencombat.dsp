# Microsoft Developer Studio Project File - Name="opencombat" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=opencombat - Win32 SDL_Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "opencombat.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "opencombat.mak" CFG="opencombat - Win32 SDL_Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "opencombat - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "opencombat - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "opencombat - Win32 SDL_Release" (based on "Win32 (x86) Application")
!MESSAGE "opencombat - Win32 SDL_Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "opencombat - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\src\opencombat"
# PROP Intermediate_Dir "..\..\src\opencombat\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\include" /I "..\..\win32" /I ".\\" /D "_WINDOWS" /D "WIN32" /D "NDEBUG" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /fo"..\..\src\opencombat\opencombat.res" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 sdlmain.lib sdl.lib ws2_32.lib dsound.lib winmm.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /pdb:"../src/opencombat/opencombat.pdb" /machine:I386 /nodefaultlib:"LIBCMT"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy ..\..\src\opencombat\*.exe ..\..\*.exe
# End Special Build Tool

!ELSEIF  "$(CFG)" == "opencombat - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\src\opencombat\Debug"
# PROP Intermediate_Dir "..\..\src\opencombat\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /GX /ZI /Od /I "..\..\include" /I "..\..\win32" /I ".\\" /D "_WINDOWS" /D "WIN32" /D "_DEBUG" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 sdlmain.lib sdl.lib ws2_32.lib dsound.lib winmm.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"LIBCMT" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy ..\..\src\opencombat\debug\*.exe ..\..\*.exe	copy   ..\..\src\opencombat\debug\*.pdb ..\..\*.pdb
# End Special Build Tool

!ELSEIF  "$(CFG)" == "opencombat - Win32 SDL_Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\src\opencombat\SDL_Release"
# PROP BASE Intermediate_Dir "..\..\src\opencombat\SDL_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\src\opencombat\SDL_Release"
# PROP Intermediate_Dir "..\..\src\opencombat\SDL_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /I "..\..\include" /I "..\..\win32" /I ".\\" /D "_WINDOWS" /D "WIN32" /D "NDEBUG" /D "_MBCS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\include" /I "..\..\win32" /I ".\\" /D "_WINDOWS" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "HAVE_SDL" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /fo"..\..\src\opencombat\opencombat.res" /d "NDEBUG"
# ADD RSC /l 0x409 /fo"..\..\src\opencombat\opencombat.res" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 sdlmain.lib sdl.lib ws2_32.lib dsound.lib winmm.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /pdb:"../src/opencombat/opencombat.pdb" /machine:I386 /nodefaultlib:"LIBCMT"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 sdlmain.lib sdl.lib ws2_32.lib dsound.lib winmm.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /pdb:"../src/opencombat/opencombat.pdb" /machine:I386 /nodefaultlib:"LIBCMT"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy ..\..\src\opencombat\SDL_Release\*.exe ..\..\*.exe
# End Special Build Tool

!ELSEIF  "$(CFG)" == "opencombat - Win32 SDL_Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "opencombat___Win32_SDL_Debug0"
# PROP BASE Intermediate_Dir "opencombat___Win32_SDL_Debug0"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\src\opencombat\SDL_Debug"
# PROP Intermediate_Dir "..\..\src\opencombat\SDL_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Zi /Od /I "..\..\include" /I "..\..\win32" /I ".\\" /D "_WINDOWS" /D "WIN32" /D "_DEBUG" /D "_MBCS" /FD /GZ /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MDd /W3 /GX /ZI /Od /I "..\..\include" /I "..\..\win32" /I ".\\" /D "_WINDOWS" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "HAVE_SDL" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 sdlmain.lib sdl.lib ws2_32.lib dsound.lib winmm.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"LIBCMT" /pdbtype:sept
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 sdlmain.lib sdl.lib ws2_32.lib dsound.lib winmm.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"LIBCMT" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy ..\..\src\opencombat\SDL_debug\*.exe ..\..\*.exe	copy   ..\..\src\opencombat\SDL_debug\*.pdb ..\..\*.pdb
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "opencombat - Win32 Release"
# Name "opencombat - Win32 Debug"
# Name "opencombat - Win32 SDL_Release"
# Name "opencombat - Win32 SDL_Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "MenuSources"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\src\opencombat\AudioMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\DisplayMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\FormatMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\GUIOptionsMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\HelpMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\HUDDialog,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\HUDDialogStack,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\InputMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\JoinMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\KeyboardMapMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\MainMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\MenuDefaultKey,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\OptionsMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\QuickKeysMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\QuitMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\SaveWorldMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerItem,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerList,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerMenu,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerStartMenu,cpp"
# End Source File
# End Group
# Begin Source File

SOURCE="..\..\src\opencombat\ActionBinding,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\AutoPilot,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\BackgroundRenderer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\BaseLocalPlayer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\bzflag,cpp"
# End Source File
# Begin Source File

SOURCE=..\bzflag.rc
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\callbacks,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\clientCommands,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ComposeDefaultKey,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ControlPanel,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\daylight,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\FlashClock,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\GuidedMissleStrategy,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\HUDRenderer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\HUDui,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\LocalPlayer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\MainWindow,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\Player,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\playing,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\RadarRenderer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\Region,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\RegionPriorityQueue,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\RemotePlayer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\RobotPlayer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\Roster,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\SceneBuilder,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\SceneRenderer,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\SegmentedShotStrategy,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerCommandKey,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerLink,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ServerListCache,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ShockWaveStrategy,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ShotPath,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ShotPathSegment,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\ShotStrategy,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\SilenceDefaultKey,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\sound,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\stars,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\TargetingUtils,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\World,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\WorldBuilder,cpp"
# End Source File
# Begin Source File

SOURCE="..\..\src\opencombat\WorldPlayer,cpp"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Menu Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\opencombat\AudioMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\FormatMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\GUIOptionsMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\HelpMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\HUDDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\InputMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\JoinMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\KeyboardMapMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\MainMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\MenuDefaultKey.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\OptionsMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\QuickKeysMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\QuitMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\SaveWorldMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerItem.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerList.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerStartMenu.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\src\opencombat\ActionBinding.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Address.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\BackgroundRenderer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BaseBuilding.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\BaseLocalPlayer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BillboardSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BoltSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BoxBuilding.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BSPSceneDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Bundle.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BundleMgr.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BZDBCache.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BzfDisplay.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BzfEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bzfgl.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bzfio.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BzfJoystick.h
# End Source File
# Begin Source File

SOURCE=..\bzflag.ico
# End Source File
# Begin Source File

SOURCE=..\..\include\BzfMedia.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bzfSDL.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BzfVisual.h
# End Source File
# Begin Source File

SOURCE=..\..\include\BzfWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bzsignal.h
# End Source File
# Begin Source File

SOURCE=..\..\include\CallbackList.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\callbacks.h
# End Source File
# Begin Source File

SOURCE=..\..\include\CommandManager.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\commands.h
# End Source File
# Begin Source File

SOURCE=..\..\include\CommandsStandard.h
# End Source File
# Begin Source File

SOURCE=..\..\include\common.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ComposeDefaultKey.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ConfigFileManager.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ControlPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\daylight.h
# End Source File
# Begin Source File

SOURCE=..\..\include\DisplayListManager.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\DisplayMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\include\DrawablesManager.h
# End Source File
# Begin Source File

SOURCE=..\..\include\EighthDBaseSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\EighthDBoxSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\EighthDimSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\EighthDPyrSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\EntryZone.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ErrorHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\include\FileManager.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Flag.h
# End Source File
# Begin Source File

SOURCE=..\..\include\FlagSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\FlagWarpSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\FlashClock.h
# End Source File
# Begin Source File

SOURCE=..\..\include\global.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\GuidedMissleStrategy.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\HUDDialogStack.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\HUDRenderer.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\HUDui.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Intersect.h
# End Source File
# Begin Source File

SOURCE=..\..\include\KeyManager.h
# End Source File
# Begin Source File

SOURCE=..\..\include\LaserSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ListServer.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\LocalPlayer.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\MainWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MediaFile.h
# End Source File
# Begin Source File

SOURCE=..\..\include\multicast.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Obstacle.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ObstacleSceneNodeGenerator.h
# End Source File
# Begin Source File

SOURCE=..\..\include\OpenGLDisplayList.h
# End Source File
# Begin Source File

SOURCE=..\..\include\OpenGLGState.h
# End Source File
# Begin Source File

SOURCE=..\..\include\OpenGLLight.h
# End Source File
# Begin Source File

SOURCE=..\..\include\OpenGLMaterial.h
# End Source File
# Begin Source File

SOURCE=..\..\include\OpenGLTexFont.h
# End Source File
# Begin Source File

SOURCE=..\..\include\OpenGLTexture.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Pack.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Ping.h
# End Source File
# Begin Source File

SOURCE=..\..\include\PlatformFactory.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\Player.h
# End Source File
# Begin Source File

SOURCE=..\..\include\PlayerState.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\playing.h
# End Source File
# Begin Source File

SOURCE="C:\Program Files\Microsoft SDK\include\PropIdl.h"
# End Source File
# Begin Source File

SOURCE=..\..\include\PyramidBuilding.h
# End Source File
# Begin Source File

SOURCE=..\..\include\QuadWallSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\RadarRenderer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Ray.h
# End Source File
# Begin Source File

SOURCE="C:\Program Files\Microsoft SDK\include\Reason.h"
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\Region.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\RegionPriorityQueue.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\RemotePlayer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\RenderNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\RobotPlayer.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\Roster.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\SceneBuilder.h
# End Source File
# Begin Source File

SOURCE=..\..\include\SceneDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\include\SceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\include\SceneRenderer.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\SegmentedShotStrategy.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerCommandKey.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerLink.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ServerListCache.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ShockWaveStrategy.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ShotPath.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ShotPathSegment.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ShotSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\ShotStrategy.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ShotUpdate.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\SilenceDefaultKey.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Singleton.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\sound.h
# End Source File
# Begin Source File

SOURCE=..\..\include\SphereSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\stars.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\StartupInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\include\StateDatabase.h
# End Source File
# Begin Source File

SOURCE=..\..\include\TankSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\TargetingUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Team.h
# End Source File
# Begin Source File

SOURCE=..\..\include\Teleporter.h
# End Source File
# Begin Source File

SOURCE=..\..\include\TextureManager.h
# End Source File
# Begin Source File

SOURCE=..\..\include\TextUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\include\TimeBomb.h
# End Source File
# Begin Source File

SOURCE=..\..\include\TimeKeeper.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ViewFrustum.h
# End Source File
# Begin Source File

SOURCE=..\..\include\WallObstacle.h
# End Source File
# Begin Source File

SOURCE=..\..\include\WallSceneNode.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\Weapon.h
# End Source File
# Begin Source File

SOURCE=..\..\include\win32.h
# End Source File
# Begin Source File

SOURCE=..\..\include\WordFilter.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\World.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\WorldBuilder.h
# End Source File
# Begin Source File

SOURCE=..\..\src\opencombat\WorldPlayer.h
# End Source File
# Begin Source File

SOURCE=..\..\src\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=..\..\src\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ZSceneDatabase.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\bzflag.ico
# End Source File
# End Group
# End Target
# End Project
