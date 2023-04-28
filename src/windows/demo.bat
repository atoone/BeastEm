@ECHO OFF
SET "PATH=.\lib;%PATH%"
beastem.exe -f monitor.rom -l 0 firmware.lst -l 23 bios.lst %*