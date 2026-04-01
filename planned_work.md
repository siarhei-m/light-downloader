# Light Downloader Modernization Plan

## Context

Light Downloader (formerly FDM-UL) is a stripped-down fork of FDM 3.9.7 (2016-era C++/MFC Windows app). It currently builds with VS 2017 (v141), targets Win 8.1 SDK, uses unsafe C string functions, bundles a minimal HTTP server, and has extensive raw pointer / `CreateThread` usage. This plan covers modernizing the build, removing the HTTP server, fixing security issues, cleaning up dead code, and modernizing C++ patterns.

---

## Phase 1: Compile with VS 2022

**Files to modify:**
- `LightDownloader.vcxproj`
- `hash/Hash.vcxproj`
- `inetfile/InetFile.vcxproj`

**Changes:**
1. `<PlatformToolset>` v141 → v143
2. `<WindowsTargetPlatformVersion>` 8.1 → 10.0
3. Keep Win32 target initially (x64 added in Phase 5)
4. Fix all new compiler warnings/errors from stricter VS 2022 checks

**Verification:** Clean build with 0 errors via VS 2022 msbuild.

---

## Phase 2: Remove HTTP Server

**Files to delete:**
- `source/vmsHttpServer.cpp/h`
- `source/vmsHttpConnection.cpp/h`
- `source/vmsHttpRequest.cpp/h`
- `source/vmsHttpResponse.cpp/h`
- `source/vmsHttpResourceContainer.cpp/h`
- `source/vmsFdmWebInterfaceServer.cpp/h` (if exists)

**Files to modify:**
- `source/Dlg_Options_Webinterface.cpp/h` — remove options page
- `source/Dlg_Options.cpp` — remove web interface tab
- `source/MainFrm.cpp` (~line 335) — remove `_httpServer` member, startup code, menu items
- `source/MainFrm.h` — remove server member variable
- `source/StdAfx.h` / `source/StdAfx.cpp` — remove server includes
- `LightDownloader.vcxproj` — remove server .cpp/.h from build
- `source/resource.h` / `fdm.rc` — remove web interface dialog resources and menu entries

**Verification:** Build succeeds; no references to vmsHttpServer remain; Options dialog has no Web Interface tab.

---

## Phase 3: Security Fixes

### 3a. Unsafe string functions
- `inetfile/fsURL.cpp` (lines 47-48, 77, 128, 130): `strcpy`/`strncpy` into fixed buffers → `strcpy_s`/`strncpy_s` or `std::string`
- `hash/vmsHash.cpp` (line 158): `sprintf` → `sprintf_s`
- Grep and fix all remaining `strcpy`, `sprintf`, `strcat`, `lstrcpy`, `wsprintf`

### 3b. Command execution hardening
- `source/vmsCommandLine.cpp` (lines 26-131, 241-283): Validate/sanitize paths before `CreateProcess`
- `source/fsScheduleMgr.cpp` (line 1066): Validate `ShellExecute` target paths

### 3c. Remove `_CRT_SECURE_NO_WARNINGS`
- Remove from all three .vcxproj preprocessor definitions
- Fix all resulting warnings (enforces 3a)

**Verification:** Build succeeds without `_CRT_SECURE_NO_WARNINGS`; no unsafe string functions remain.

---

## Phase 4: Dead Code Cleanup

1. **Commented-out code** — remove references to: BitTorrent, RTSP, Flash streams, mirrors, plugins, skins, Spider/Site Explorer, browser extensions, unused thread creation (`fsDownloadsMgr.cpp:60`)
2. **Dead `#ifdef` blocks** — remove `#if 0` and ifdefs for features that no longer exist
3. **TODO/FIXME cleanup** — address or remove:
   - `Dlg_Options_Webinterface.cpp:78` (gone after Phase 2)
   - `Dlg_CreateDownload.cpp:1075` — URL validation
   - `DownloadProperties_ProxyPage.cpp:703,742` — SOCKS controls
   - `Dlg_YtDlp.h:32` — ApplyLanguage stub
4. **Unused .rgs files** — remove ATL registration scripts if COM is no longer used

**Verification:** Grep for dead-code markers shows clean results; clean build.

---

## Phase 5: Code Simplification & Improvements

### 5a. Smart pointers
Replace raw `new`/`delete` with `std::unique_ptr`/`std::shared_ptr` in key classes:
- `fsDownloadsMgr.cpp` (heaviest raw pointer usage)
- Download task objects
- Incremental: file-by-file, verify build after each

### 5b. Modern threading
Replace `CreateThread` + `CRITICAL_SECTION` with `std::thread` + `std::mutex`:
- 28+ CreateThread call sites across the codebase
- Priority: download manager, scheduler, network code
- Requires `/std:c++17` in vcxproj

### 5c. String safety
Migrate hot-path URL handling from `char[]` buffers to `std::string`.

### 5d. x64 build target
Add x64 platform configuration to all three .vcxproj files.

### 5e. Static analysis
Enable `/analyze` in VS 2022 and fix findings.

### 5f. WinInet → WinHTTP migration
Replace deprecated WinInet with modern WinHTTP API in `inetfile/` library. Improves TLS 1.2/1.3 support. Key files:
- `inetfile/fsInternetSession.cpp`
- `inetfile/fsHttpFile.cpp`
- `inetfile/fsFtpFile.cpp`

---

## Execution Order

| Step | Phase | Risk | Effort |
|------|-------|------|--------|
| 1 | VS 2022 build | Low | Small |
| 2 | Remove HTTP server | Low | Medium |
| 3 | Security fixes | Medium | Medium |
| 4 | Dead code cleanup | Low | Small-Medium |
| 5 | Simplification | Medium | Large (incremental) |
