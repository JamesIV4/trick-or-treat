// Windows cannot load a GDExtension DLL from Godot's embedded pack. The release
// builder appends the exported game and DLL to this launcher, followed by two
// little-endian 64-bit sizes and the eight-byte marker below. Each run extracts
// them to a private temporary directory, then removes them after the game exits.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr char BUNDLE_MARKER[] = "TOTPKG01";
constexpr DWORD TRAILER_SIZE = 24;
constexpr wchar_t DLL_NAME[] = L"totnative.windows.template_release.x86_64.dll";

void show_error(const wchar_t *message, DWORD error = 0) {
	std::wstring text = message;
	if (error != 0) {
		wchar_t *details = nullptr;
		if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
				reinterpret_cast<wchar_t *>(&details), 0, nullptr)) {
			text += L"\n\n";
			text += details;
			LocalFree(details);
		}
	}
	MessageBoxW(nullptr, text.c_str(), L"Halloween: Trick or Treat!", MB_OK | MB_ICONERROR);
}

uint64_t read_u64_le(const unsigned char *bytes) {
	uint64_t value = 0;
	for (int i = 0; i < 8; ++i) {
		value |= uint64_t(bytes[i]) << (8 * i);
	}
	return value;
}

bool extract_file(HANDLE source, const std::wstring &path, uint64_t size) {
	HANDLE dest = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL, nullptr);
	if (dest == INVALID_HANDLE_VALUE) {
		return false;
	}
	std::vector<char> buffer(1024 * 1024);
	bool ok = true;
	while (size > 0) {
		const DWORD amount = static_cast<DWORD>(std::min<uint64_t>(size, buffer.size()));
		DWORD read = 0;
		if (!ReadFile(source, buffer.data(), amount, &read, nullptr)) {
			ok = false;
			break;
		}
		if (read != amount) {
			SetLastError(ERROR_HANDLE_EOF);
			ok = false;
			break;
		}
		DWORD offset = 0;
		while (offset < read) {
			DWORD written = 0;
			if (!WriteFile(dest, buffer.data() + offset, read - offset, &written, nullptr)) {
				ok = false;
				break;
			}
			if (written == 0) {
				SetLastError(ERROR_WRITE_FAULT);
				ok = false;
				break;
			}
			offset += written;
		}
		if (!ok) {
			break;
		}
		size -= read;
	}
	const DWORD error = GetLastError();
	CloseHandle(dest);
	if (!ok) {
		SetLastError(error);
	}
	return ok;
}

// Preserve the original argument text, including quotes, for the Godot child.
const wchar_t *arguments_after_executable() {
	const wchar_t *args = GetCommandLineW();
	if (*args == L'"') {
		++args;
		while (*args && *args != L'"') ++args;
		if (*args) ++args;
	} else {
		while (*args && *args != L' ' && *args != L'\t') ++args;
	}
	return args;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	wchar_t module_path[32768];
	const DWORD module_length = GetModuleFileNameW(nullptr, module_path, 32768);
	if (module_length == 0 || module_length >= 32768) {
		show_error(L"Could not find the game executable.", GetLastError());
		return 1;
	}
	HANDLE source = CreateFileW(module_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (source == INVALID_HANDLE_VALUE) {
		show_error(L"Could not open the game executable.", GetLastError());
		return 1;
	}
	LARGE_INTEGER file_size = {}, trailer_pos = {};
	unsigned char trailer[TRAILER_SIZE];
	DWORD read = 0;
	bool valid = GetFileSizeEx(source, &file_size) && file_size.QuadPart > TRAILER_SIZE;
	if (valid) {
		trailer_pos.QuadPart = file_size.QuadPart - TRAILER_SIZE;
		valid = SetFilePointerEx(source, trailer_pos, nullptr, FILE_BEGIN) &&
				ReadFile(source, trailer, TRAILER_SIZE, &read, nullptr) && read == TRAILER_SIZE &&
				std::memcmp(trailer + 16, BUNDLE_MARKER, 8) == 0;
	}
	const uint64_t game_size = valid ? read_u64_le(trailer) : 0;
	const uint64_t dll_size = valid ? read_u64_le(trailer + 8) : 0;
	const uint64_t available = valid ? static_cast<uint64_t>(file_size.QuadPart - TRAILER_SIZE) : 0;
	valid = valid && game_size > 0 && dll_size > 0 && game_size < available &&
			dll_size <= available - game_size;
	if (!valid) {
		CloseHandle(source);
		show_error(L"The game executable is incomplete or damaged. Please copy it again.");
		return 1;
	}
	LARGE_INTEGER payload_pos = {};
	payload_pos.QuadPart = static_cast<LONGLONG>(available - game_size - dll_size);
	if (!SetFilePointerEx(source, payload_pos, nullptr, FILE_BEGIN)) {
		const DWORD error = GetLastError();
		CloseHandle(source);
		show_error(L"Could not read the game executable.", error);
		return 1;
	}
	wchar_t temp_path[32768];
	const DWORD temp_length = GetTempPathW(32768, temp_path);
	if (temp_length == 0 || temp_length >= 32768) {
		const DWORD error = GetLastError();
		CloseHandle(source);
		show_error(L"Could not find the temporary folder needed to start the game.", error);
		return 1;
	}
	std::wstring directory;
	bool created = false;
	for (int i = 0; i < 100; ++i) {
		directory = std::wstring(temp_path) + L"TrickOrTreat-" + std::to_wstring(GetCurrentProcessId()) +
				L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(i);
		if (CreateDirectoryW(directory.c_str(), nullptr)) {
			created = true;
			break;
		}
		if (GetLastError() != ERROR_ALREADY_EXISTS) break;
	}
	if (!created) {
		const DWORD error = GetLastError();
		CloseHandle(source);
		show_error(L"Could not create the temporary folder needed to start the game.", error);
		return 1;
	}
	const std::wstring game = directory + L"\\Game.exe";
	const std::wstring dll = directory + L"\\" + DLL_NAME;
	const bool extracted = extract_file(source, game, game_size) && extract_file(source, dll, dll_size);
	const DWORD extraction_error = GetLastError();
	CloseHandle(source);
	if (!extracted) {
		DeleteFileW(game.c_str());
		DeleteFileW(dll.c_str());
		RemoveDirectoryW(directory.c_str());
		show_error(L"Could not prepare the game files in the temporary folder.", extraction_error);
		return 1;
	}
	std::wstring command = L"\"" + game + L"\"" + arguments_after_executable();
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	const bool launched = CreateProcessW(game.c_str(), &command[0], nullptr, nullptr, TRUE, 0,
			nullptr, nullptr, &startup, &process) != 0;
	const DWORD launch_error = GetLastError();
	DWORD exit_code = 1;
	if (launched) {
		CloseHandle(process.hThread);
		WaitForSingleObject(process.hProcess, INFINITE);
		GetExitCodeProcess(process.hProcess, &exit_code);
		CloseHandle(process.hProcess);
	}
	DeleteFileW(game.c_str());
	DeleteFileW(dll.c_str());
	RemoveDirectoryW(directory.c_str());
	if (!launched) {
		show_error(L"Could not start the game.", launch_error);
	}
	return static_cast<int>(exit_code);
}
