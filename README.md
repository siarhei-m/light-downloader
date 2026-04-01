# Light Downloader

A lightweight, portable download manager for Windows with multi-part/segmented downloads.

Based on [Free Download Manager 3.9.7](https://sourceforge.net/p/freedownload/code/HEAD/tree/) via [FDM-UL](https://github.com/59de44955ebd/FDM-UL), modernized to C++20, Visual Studio 2022, [libcurl](https://curl.se/) and [pugixml](https://pugixml.org/).

![Light Downloader](screenshots/LightDownloader.png)

## Building

Requires:
- Visual Studio 2022 with C++ MFC for v143 build tools
- [vcpkg](https://vcpkg.io/) for dependencies (libcurl, pugixml)

### Setup vcpkg (one-time)

```bat
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\vcpkg
```

### Install dependencies

```bat
cd C:\src\light-downloader
C:\vcpkg\vcpkg install --triplet x86-windows-static --x-install-root=vcpkg_installed
C:\vcpkg\vcpkg install --triplet x64-windows-static --x-install-root=vcpkg_installed
```

### Build

```bat
msbuild LightDownloader.sln /p:Configuration=Release /p:Platform=Win32
```

Or open `LightDownloader.sln` in Visual Studio 2022.

## License

GNU General Public License v3.0
