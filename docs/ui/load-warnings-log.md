# Non-Blocking Load Warnings & Log Rotation

## 1. Pattern & Key Files
- `util/load_warnings_log.{h,cpp}` — pure line splitting (`SplitNonEmptyWarningLines`), formatting (`FormatLoadWarningsBlock`), and rotating log append (`TryAppendLoadWarnings`).
- `util/file_system.{h,cpp}` — `FileSystem::GetLoadWarningsLogPaths()` (cross-platform exec-dir primary + `GetLocalDirectory()` fallback).
- `ui/dialog_util.{h,cpp}` — `DialogUtil::LoadWarningsDialog(parent, title, warnings)` routes to `ListDialog` or `TryAppendLoadWarnings` based on `Config::SHOW_LOAD_WARNINGS_DIALOG`.

## 2. When to Use It
Do:
- Route client/map/monster load warning lists through `DialogUtil::LoadWarningsDialog(parent, title, warnings)` instead of `ListDialog`.
- Keep `load_warnings_log.{h,cpp}` free of wxWidgets dependencies so `test_load_warnings_log` runs standalone.
Don't:
- Don't pass `wxString::ToStdWstring()` to `std::filesystem::path` outside `#ifdef _WIN32`; use `FileSystem` path helpers (`ToUTF8()` on POSIX).
- Don't insert new `Config::Key` entries in the middle of `settings.h`; always append before `LAST`.

## 3. Minimal Usage
```cpp
if (g_version.LoadVersion(target->getID(), error, warnings)) {
	DialogUtil::LoadWarningsDialog(parent, "Warnings", warnings);
}
```

## 4. Invariants & Pitfalls
- Blank-only entries (`""`, `"\n"`, `"\r\n"`) are dropped in both dialog and file-log modes so neither shows an empty window nor writes a bodyless header.
- Logs rotate to `load-warnings.log.1` above `kMaxLoadWarningsLogBytes` (5 MiB) and fall back to `ListDialog` if all candidate paths fail.
