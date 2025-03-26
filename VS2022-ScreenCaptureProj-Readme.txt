
1. Verify VS2022 project settings

Project Config settings:
------------------------
Configuration properties
	----> VC++ Directories -> Include Directories:
      Add:  C:\libva\packages\Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2\build\native\x64\include\;


Configuration properties
	----> VC++ Directories -> Library Directories:
      Add: C:\libva\packages\Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2\build\native\x64\bin;


Configuration properties
	----> Linker -> Input:
      Add: va_win32.lib;va.lib;


2. Install Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2 package in VS2022 project
	On VS2022 Tools -> NuGet Package Manager -> Manage Nuget Packages for Solution
	Serach for "Microsoft.Direct3D.VideoAccelerationCompatibilityPack" in search window
	Select and install
	This will install the packages in the project directory

3. Copy Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2 packages drim VS2022 project directory to  C:\libva\packages\

4. Set required environmant system variables
Set system env variables:
LIBVA_DRIVER_NAME : vaon12
LIBVA_DRIVERS_PATH: C:\libva\packages\Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2\build\native\x64\bin\
 
5. Open new cmd and check
echo %LIBVA_DRIVER_NAME%
echo %LIBVA_DRIVERS_PATH%
 Note: Variables set in step 4 should see here
 

6. Restart VS2022 to get systen env variables 