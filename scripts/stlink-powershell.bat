@echo off
:: Ensure the script is running as administrator
powershell -Command "if (!([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] 'Administrator')) { echo 'Please run this script as Administrator.'; exit 1; }"

:: List all USB devices
usbipd list

:: Extract the BUSID of the ST-LINK device from the Connected section
set "BUSID="
for /f "tokens=1,2 delims= " %%a in ('usbipd list ^| findstr /r /c:"^[0-9]-[0-9]" ^| findstr "STM32 STLink"') do (
    set "BUSID=%%a"
)

:: Check if BUSID was set
if "%BUSID%"=="" (
    echo ST-LINK device not found. Exiting...
    exit /b 1
)

:: Bind the ST-LINK device
echo Binding ST-LINK device with BUSID: %BUSID%
powershell -Command "usbipd bind --busid %BUSID%"

:: Verify that the device is shared
usbipd list

:: Attach the ST-LINK device to WSL
echo Attaching ST-LINK device with BUSID: %BUSID% to WSL...
powershell -Command "usbipd attach --wsl --busid %BUSID%"

:: Wait for user input before detaching
pause

:: Detach the ST-LINK device
echo Detaching ST-LINK device with BUSID: %BUSID%
powershell -Command "usbipd detach --busid %BUSID%"

echo Done.
