
:: download https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.7z
:: place it at C://program_files/boost_1_80_0

:: open up cmd as administrator

:: low key had to follow these instructions to get gcc to show up as an option for boost (fk vs)
:: only weird part is I installed mingw through chocolatey. so replace
:: C:\Program Files\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64 with
:: C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64

:: https://gist.github.com/zrsmithson/0b72e0cb58d0cb946fc48b5c88511da8

cd C:\Program Files\boost_1_80_0\tools\build
.\bootstrap.bat gcc
.\b2 --prefix="C:\Program Files\boost_1_80_0" install
set PATH="%PATH%;C:\Program Files\boost_1_80_0"
cd ..\..
.\b2 toolset=gcc --with-system --with-thread --with-date_time --with-regex --with-serialization stage

:: I have no idea why, but this kept opening up tabs on sublime...