// CantoCandidate - portable Cantonese candidate input helper for Windows 11.
// Not affiliated with, endorsed by, or supported by Google.
// Build: x86_64-w64-mingw32-g++ -std=c++17 -O2 -s -mwindows src/CantoCandidate.cpp -o dist/CantoCandidate.exe -lwinhttp -luser32 -lgdi32 -lshell32

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <ole2.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

constexpr UINT WM_CANDIDATES_READY = WM_APP + 1;
constexpr UINT WM_TRAY = WM_APP + 2;
constexpr UINT TIMER_STATUS = 42;
constexpr UINT TIMER_REQUEST = 43;
constexpr UINT TIMER_CLIPBOARD_RESTORE = 44;
constexpr size_t MAX_COMPOSITION_CHARS = 64;
constexpr size_t MAX_RESPONSE_BYTES = 1024 * 1024;
constexpr int HOTKEY_ID = 100;
constexpr UINT TRAY_ID = 1;
constexpr size_t PAGE_SIZE = 9;

HWND gMain = nullptr;
HWND gCandidateWindow = nullptr;
HHOOK gHook = nullptr;
HINSTANCE gInstance = nullptr;
bool gEnabled = false;
bool gEnglishMode = false;
bool gShiftDown = false;
bool gShiftUsed = false;
ULONGLONG gLastRequestTick = 0;
std::wstring gPendingComposition;
size_t gCandidatePage = 0;
size_t gCandidateFocus = 0;
std::wstring gComposition;
std::vector<std::wstring> gCandidates;
std::wstring gStatus;
std::wstring gFuzzyHint;

struct Settings {
    bool followCaret = true;
    bool preserveClipboard = true;
    bool singleShiftToggle = true;
};
Settings gSettings;
IDataObject* gClipboardBackup = nullptr;
DWORD gClipboardSequenceAfterPaste = 0;

void ResizeAndShowCandidate();
void UpdateTrayIcon();

struct CandidateResult {
    std::wstring composition;
    std::vector<std::wstring> candidates;
    std::wstring fuzzyHint;
    std::wstring error;
};

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (count <= 0) return L"";
    std::wstring output(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), count);
    return output;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return "";
    std::string output(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), count, nullptr, nullptr);
    return output;
}

std::string UrlEncodeAscii(const std::wstring& value) {
    std::string raw = WideToUtf8(value);
    std::ostringstream encoded;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : raw) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            encoded << static_cast<char>(c);
        } else {
            encoded << '%' << hex[(c >> 4) & 0x0F] << hex[c & 0x0F];
        }
    }
    return encoded.str();
}

std::wstring CompactLower(const std::wstring& value);

bool IsSafeDataPath(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0;
}

std::wstring DataFilePath(const wchar_t* name) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = L'\0';
    return std::wstring(path) + name;
}

std::wstring SystemNotepadPath() {
    wchar_t windowsDirectory[MAX_PATH]{};
    UINT length = GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return L"notepad.exe";
    return std::wstring(windowsDirectory) + L"\\System32\\notepad.exe";
}

std::string ReadUtf8File(const std::wstring& path) {
    if (!IsSafeDataPath(path)) return "";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return "";
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 1024 * 1024) { CloseHandle(file); return ""; }
    std::string data(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!data.empty()) ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
    CloseHandle(file);
    data.resize(read);
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB && static_cast<unsigned char>(data[2]) == 0xBF) data.erase(0, 3);
    return data;
}

bool WriteUtf8File(const std::wstring& path, const std::string& data, DWORD creation = CREATE_ALWAYS) {
    if (!IsSafeDataPath(path) || data.size() > 1024 * 1024) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, creation, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = data.empty() || WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == data.size();
}

bool ReadSettingValue(const std::wstring& data, const std::wstring& key, bool fallback) {
    std::wstringstream lines(data);
    std::wstring line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;
        size_t equals = line.find(L'=');
        if (equals == std::wstring::npos) continue;
        std::wstring name = line.substr(0, equals);
        std::wstring value = line.substr(equals + 1);
        if (name != key) continue;
        if (value == L"1") return true;
        if (value == L"0") return false;
        return fallback;
    }
    return fallback;
}

void EnsureSettingsFile() {
    const std::string starter = "; CantoCandidate settings (0=off, 1=on)\nfollow_caret=1\npreserve_clipboard=1\nsingle_shift_toggle=1\n";
    WriteUtf8File(DataFilePath(L"settings.ini"), starter, CREATE_NEW);
}

void LoadSettings() {
    EnsureSettingsFile();
    std::wstring data = Utf8ToWide(ReadUtf8File(DataFilePath(L"settings.ini")));
    if (data.empty()) return;
    gSettings.followCaret = ReadSettingValue(data, L"follow_caret", true);
    gSettings.preserveClipboard = ReadSettingValue(data, L"preserve_clipboard", true);
    gSettings.singleShiftToggle = ReadSettingValue(data, L"single_shift_toggle", true);
}

struct HistoryEntry {
    std::wstring input;
    std::wstring word;
    int count = 0;
};

std::vector<HistoryEntry> LoadHistory() {
    std::vector<HistoryEntry> entries;
    std::wstring data = Utf8ToWide(ReadUtf8File(DataFilePath(L"history.tsv")));
    std::wstringstream lines(data);
    std::wstring line;
    while (std::getline(lines, line)) {
        size_t first = line.find(L'\t');
        size_t second = first == std::wstring::npos ? std::wstring::npos : line.find(L'\t', first + 1);
        if (first == std::wstring::npos || second == std::wstring::npos) continue;
        try {
            int count = std::max(0, std::stoi(line.substr(second + 1)));
            if (count > 0) entries.push_back({CompactLower(line.substr(0, first)), line.substr(first + 1, second - first - 1), count});
        } catch (...) {}
    }
    return entries;
}

void SaveHistory(const std::vector<HistoryEntry>& entries) {
    std::wstring data;
    for (const auto& entry : entries) data += entry.input + L"\t" + entry.word + L"\t" + std::to_wstring(entry.count) + L"\n";
    WriteUtf8File(DataFilePath(L"history.tsv"), WideToUtf8(data));
}

std::vector<std::wstring> LoadCustomWords(const std::wstring& composition) {
    std::vector<std::wstring> words;
    std::wstring data = Utf8ToWide(ReadUtf8File(DataFilePath(L"custom_dictionary.tsv")));
    std::wstringstream lines(data);
    std::wstring line;
    std::wstring key = CompactLower(composition);
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == L'#') continue;
        size_t tab = line.find(L'\t');
        if (tab == std::wstring::npos) continue;
        std::wstring input = CompactLower(line.substr(0, tab));
        std::wstring word = line.substr(tab + 1);
        if (input == key && !word.empty() && std::find(words.begin(), words.end(), word) == words.end()) words.push_back(word);
    }
    return words;
}

int HistoryScore(const std::vector<HistoryEntry>& history, const std::wstring& composition, const std::wstring& word) {
    std::wstring key = CompactLower(composition);
    for (const auto& entry : history) if (entry.input == key && entry.word == word) return entry.count;
    return 0;
}

std::vector<std::wstring> PersonalizeCandidates(const std::wstring& composition, const std::vector<std::wstring>& remote) {
    std::vector<std::wstring> result = LoadCustomWords(composition);
    std::vector<HistoryEntry> history = LoadHistory();
    std::vector<std::pair<std::wstring, int>> ranked;
    for (const auto& word : remote) {
        if (std::find(result.begin(), result.end(), word) == result.end()) ranked.push_back({word, HistoryScore(history, composition, word)});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& item : ranked) result.push_back(item.first);
    return result;
}

void RecordHistory(const std::wstring& composition, const std::wstring& word) {
    if (composition.empty() || word.empty()) return;
    std::vector<HistoryEntry> history = LoadHistory();
    std::wstring key = CompactLower(composition);
    for (auto& entry : history) {
        if (entry.input == key && entry.word == word) { entry.count = std::min(entry.count + 1, 1000000); SaveHistory(history); return; }
    }
    history.push_back({key, word, 1});
    SaveHistory(history);
}

void EnsureDictionaryFile() {
    const std::string starter = "# CantoCandidate custom dictionary (UTF-8)\n# Format: jyutping<TAB>word\nneihou\t你好\n";
    WriteUtf8File(DataFilePath(L"custom_dictionary.tsv"), starter, CREATE_NEW);
}

void ClearHistoryFile() {
    std::wstring path = DataFilePath(L"history.tsv");
    if (IsSafeDataPath(path)) DeleteFileW(path.c_str());
}

bool ReadJsonString(const std::string& input, size_t& position, std::string& output) {
    output.clear();
    while (position < input.size() && isspace(static_cast<unsigned char>(input[position]))) ++position;
    if (position >= input.size() || input[position] != '"') return false;
    ++position;
    while (position < input.size()) {
        char c = input[position++];
        if (c == '"') return true;
        if (c != '\\') {
            output.push_back(c);
            continue;
        }
        if (position >= input.size()) return false;
        char escaped = input[position++];
        switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                if (position + 4 > input.size()) return false;
                unsigned value = 0;
                for (int i = 0; i < 4; ++i) {
                    char h = input[position++];
                    value <<= 4;
                    if (h >= '0' && h <= '9') value |= h - '0';
                    else if (h >= 'a' && h <= 'f') value |= h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') value |= h - 'A' + 10;
                    else return false;
                }
                if (value <= 0x7F) output.push_back(static_cast<char>(value));
                else if (value <= 0x7FF) {
                    output.push_back(static_cast<char>(0xC0 | ((value >> 6) & 0x1F)));
                    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
                } else {
                    output.push_back(static_cast<char>(0xE0 | ((value >> 12) & 0x0F)));
                    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
                    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
                }
                break;
            }
            default: return false;
        }
    }
    return false;
}

std::wstring CompactLower(const std::wstring& value) {
    std::wstring output;
    for (wchar_t c : value) {
        if (!iswspace(c)) output.push_back(towlower(c));
    }
    return output;
}

std::wstring ParseFirstAnnotation(const std::string& response) {
    const std::string marker = "\"annotation\":[";
    size_t position = response.find(marker);
    if (position == std::string::npos) return L"";
    position += marker.size();
    std::string raw;
    if (!ReadJsonString(response, position, raw)) return L"";
    return Utf8ToWide(raw);
}

std::vector<std::wstring> ParseCandidates(const std::string& response, const std::wstring& composition) {
    std::vector<std::wstring> values;
    std::string marker = "[[\"" + WideToUtf8(composition) + "\",[";
    size_t start = response.find(marker);
    if (start == std::string::npos) return values;
    size_t position = start + marker.size();
    while (position < response.size() && values.size() < 45) {
        while (position < response.size() && (response[position] == ',' || isspace(static_cast<unsigned char>(response[position])))) ++position;
        if (position >= response.size() || response[position] == ']') break;
        std::string raw;
        if (!ReadJsonString(response, position, raw)) break;
        std::wstring candidate = Utf8ToWide(raw);
        if (!candidate.empty()) values.push_back(candidate);
    }
    return values;
}

bool DownloadCandidates(const std::wstring& composition, CandidateResult& result) {
    std::string targetUtf8 = "/request?text=" + UrlEncodeAscii(composition) +
        "&itc=yue-hant-t-i0-und&num=45&cp=0&cs=1&ie=utf-8&oe=utf-8";
    std::wstring target = Utf8ToWide(targetUtf8);
    HINTERNET session = WinHttpOpen(L"CantoCandidate/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { result.error = L"無法建立網絡連線"; return false; }
    WinHttpSetTimeouts(session, 3000, 3000, 4000, 4000);
    HINTERNET connection = WinHttpConnect(session, L"inputtools.google.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) { WinHttpCloseHandle(session); result.error = L"無法連接候選字服務"; return false; }
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
        result.error = L"無法建立候選字請求"; return false;
    }
    bool sent = WinHttpSendRequest(request, L"Accept: application/json\r\n", -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);
    if (!sent) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
        result.error = L"候選字服務沒有回應"; return false;
    }
    std::string body;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        if (available > MAX_RESPONSE_BYTES || body.size() > MAX_RESPONSE_BYTES - available) {
            WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
            result.error = L"候選字服務回應過大"; return false;
        }
        std::vector<char> buffer(available + 1, 0);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) break;
        body.append(buffer.data(), read);
        available = 0;
    }
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    if (body.find("SUCCESS") == std::string::npos) {
        result.error = L"候選字服務傳回無效資料"; return false;
    }
    result.candidates = PersonalizeCandidates(composition, ParseCandidates(body, composition));
    std::wstring firstAnnotation = ParseFirstAnnotation(body);
    if (!firstAnnotation.empty() && CompactLower(firstAnnotation) != CompactLower(composition)) {
        result.fuzzyHint = firstAnnotation;
    }
    if (result.candidates.empty()) {
        result.error = L"找不到候選字"; return false;
    }
    return true;
}

DWORD WINAPI CandidateThread(LPVOID parameter) {
    std::wstring* input = static_cast<std::wstring*>(parameter);
    CandidateResult* result = new CandidateResult();
    result->composition = *input;
    delete input;
    DownloadCandidates(result->composition, *result);
    if (!PostMessage(gMain, WM_CANDIDATES_READY, 0, reinterpret_cast<LPARAM>(result))) delete result;
    return 0;
}

void RequestCandidates() {
    if (gComposition.empty()) return;
    constexpr ULONGLONG debounceMs = 120;
    ULONGLONG now = GetTickCount64();
    if (now - gLastRequestTick < debounceMs) {
        gPendingComposition = gComposition;
        SetTimer(gMain, TIMER_REQUEST, static_cast<UINT>(debounceMs - (now - gLastRequestTick)), nullptr);
        return;
    }
    gPendingComposition.clear();
    gLastRequestTick = now;
    std::wstring* request = new std::wstring(gComposition);
    HANDLE thread = CreateThread(nullptr, 0, CandidateThread, request, 0, nullptr);
    if (thread) CloseHandle(thread); else delete request;
}

size_t PageCount() {
    return gCandidates.empty() ? 1 : (gCandidates.size() + PAGE_SIZE - 1) / PAGE_SIZE;
}

size_t CurrentPageStart() {
    return gCandidatePage * PAGE_SIZE;
}

size_t CurrentPageSize() {
    size_t start = CurrentPageStart();
    return start < gCandidates.size() ? std::min(PAGE_SIZE, gCandidates.size() - start) : 0;
}

std::wstring CandidateDisplayText() {
    if (!gStatus.empty()) return gStatus;
    if (gComposition.empty()) return L"";
    std::wstring display = gEnglishMode ? L"EN　" : L"中　";
    display += L"粵語：" + gComposition;
    if (!gFuzzyHint.empty()) display += L"　容錯讀音：" + gFuzzyHint;
    display += L"　[" + std::to_wstring(gCandidatePage + 1) + L"/" + std::to_wstring(PageCount()) + L"]\n";
    if (gCandidates.empty()) return display + L"搜尋中…";
    size_t start = CurrentPageStart();
    size_t count = CurrentPageSize();
    for (size_t slot = 0; slot < count; ++slot) {
        size_t index = start + slot;
        bool focused = slot == gCandidateFocus;
        if (focused) display += L"【";
        display += std::to_wstring(slot + 1) + L"." + gCandidates[index];
        if (focused) display += L"】";
        display += L"　";
    }
    return display;
}

bool TryGetCaretAnchor(POINT& point) {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    DWORD threadId = GetWindowThreadProcessId(foreground, nullptr);
    GUITHREADINFO info{};
    info.cbSize = sizeof(info);
    if (!threadId || !GetGUIThreadInfo(threadId, &info) || !info.hwndCaret) return false;
    point.x = info.rcCaret.left;
    point.y = info.rcCaret.bottom;
    return ClientToScreen(info.hwndCaret, &point) != FALSE;
}

void ResizeAndShowCandidate() {
    if (!gCandidateWindow) return;
    std::wstring display = CandidateDisplayText();
    if (display.empty()) { ShowWindow(gCandidateWindow, SW_HIDE); return; }
    POINT point{};
    if (!gSettings.followCaret || !TryGetCaretAnchor(point)) GetCursorPos(&point);
    int width = 820;
    int height = 82;
    int x = std::max(4L, point.x - 20L);
    int y = point.y + 24;
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    if (x + width > work.right) x = std::max(4L, work.right - width - 4);
    if (y + height > work.bottom) y = point.y - height - 12;
    SetWindowPos(gCandidateWindow, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(gCandidateWindow, nullptr, TRUE);
}

void ClearComposition() {
    gComposition.clear();
    gCandidates.clear();
    gStatus.clear();
    gFuzzyHint.clear();
    gPendingComposition.clear();
    KillTimer(gMain, TIMER_REQUEST);
    gCandidatePage = 0;
    gCandidateFocus = 0;
    ShowWindow(gCandidateWindow, SW_HIDE);
}

void ReleaseClipboardBackup() {
    KillTimer(gMain, TIMER_CLIPBOARD_RESTORE);
    if (gClipboardBackup) {
        gClipboardBackup->Release();
        gClipboardBackup = nullptr;
    }
    gClipboardSequenceAfterPaste = 0;
}

void RestoreClipboardIfUnchanged() {
    KillTimer(gMain, TIMER_CLIPBOARD_RESTORE);
    if (gClipboardBackup && GetClipboardSequenceNumber() == gClipboardSequenceAfterPaste) {
        OleSetClipboard(gClipboardBackup);
    }
    ReleaseClipboardBackup();
}

void PasteText(const std::wstring& text) {
    if (text.empty()) return;
    RestoreClipboardIfUnchanged();
    if (gSettings.preserveClipboard) OleGetClipboard(&gClipboardBackup);
    if (!OpenClipboard(nullptr)) { ReleaseClipboardBackup(); return; }
    EmptyClipboard();
    SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* destination = GlobalLock(memory);
        if (destination) {
            memcpy(destination, text.c_str(), bytes);
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
    INPUT input[4]{};
    input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = VK_CONTROL;
    input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = 'V';
    input[2].type = INPUT_KEYBOARD; input[2].ki.wVk = 'V'; input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3].type = INPUT_KEYBOARD; input[3].ki.wVk = VK_CONTROL; input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, input, sizeof(INPUT));
    if (gClipboardBackup) {
        gClipboardSequenceAfterPaste = GetClipboardSequenceNumber();
        SetTimer(gMain, TIMER_CLIPBOARD_RESTORE, 150, nullptr);
    }
}

void SelectCandidate(size_t index) {
    if (index >= gCandidates.size()) return;
    std::wstring selected = gCandidates[index];
    std::wstring input = gComposition;
    ClearComposition();
    RecordHistory(input, selected);
    PasteText(selected);
}

void UpdateTrayIcon() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = gMain;
    data.uID = TRAY_ID;
    data.uFlags = NIF_ICON | NIF_TIP;
    data.hIcon = LoadIcon(nullptr, gEnabled ? IDI_INFORMATION : IDI_APPLICATION);
    if (!gEnabled) wcscpy_s(data.szTip, L"粵語候選字：關閉（Ctrl+Alt+G 開啟）");
    else if (gEnglishMode) wcscpy_s(data.szTip, L"粵語候選字：EN（Shift 切換至中文）");
    else wcscpy_s(data.szTip, L"粵語候選字：中（Shift 切換至英文）");
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void ToggleInput() {
    gEnabled = !gEnabled;
    ClearComposition();
    gStatus = gEnabled ? (gEnglishMode ? L"EN　英文直接輸入" : L"中　粵語拼音輸入") : L"粵語候選字：已關閉";
    ResizeAndShowCandidate();
    SetTimer(gMain, TIMER_STATUS, 1200, nullptr);
    UpdateTrayIcon();
}

LRESULT CALLBACK CandidateWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(hwnd, &paint);
        RECT rect{}; GetClientRect(hwnd, &rect);
        HBRUSH background = CreateSolidBrush(RGB(31, 41, 55));
        FillRect(hdc, &rect, background);
        DeleteObject(background);
        HFONT font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft JhengHei UI");
        HGDIOBJ previous = SelectObject(hdc, font);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(248, 250, 252));
        std::wstring display = CandidateDisplayText();
        RECT textRect{16, 9, rect.right - 16, rect.bottom - 8};
        DrawTextW(hdc, display.c_str(), static_cast<int>(display.size()), &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        SelectObject(hdc, previous);
        DeleteObject(font);
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void MoveCandidatePage(int direction) {
    size_t pages = PageCount();
    if (pages <= 1) return;
    if (direction < 0 && gCandidatePage > 0) --gCandidatePage;
    if (direction > 0 && gCandidatePage + 1 < pages) ++gCandidatePage;
    gCandidateFocus = 0;
    ResizeAndShowCandidate();
}

void MoveCandidateFocus(int direction) {
    size_t count = CurrentPageSize();
    if (count == 0) return;
    if (direction < 0 && gCandidateFocus > 0) --gCandidateFocus;
    if (direction > 0 && gCandidateFocus + 1 < count) ++gCandidateFocus;
    ResizeAndShowCandidate();
}

void ToggleLanguageMode() {
    gEnglishMode = !gEnglishMode;
    ClearComposition();
    gStatus = gEnglishMode ? L"EN　英文直接輸入" : L"中　粵語拼音輸入";
    ResizeAndShowCandidate();
    SetTimer(gMain, TIMER_STATUS, 1200, nullptr);
    UpdateTrayIcon();
}

std::wstring ChinesePunctuation(DWORD vk, bool shift) {
    switch (vk) {
        case VK_OEM_PERIOD: return L"。";
        case VK_OEM_COMMA: return L"，";
        case VK_OEM_1: return shift ? L"：" : L"；";
        case VK_OEM_2: return shift ? L"？" : L"／";
        case VK_OEM_3: return shift ? L"～" : L"`";
        case VK_OEM_4: return L"「";
        case VK_OEM_5: return L"、";
        case VK_OEM_6: return L"」";
        case VK_OEM_7: return shift ? L"「" : L"、";
        case VK_OEM_MINUS: return shift ? L"＿" : L"－";
        case VK_OEM_PLUS: return shift ? L"＋" : L"＝";
        case '9': return shift ? L"（" : L"";
        case '0': return shift ? L"）" : L"";
        case '1': return shift ? L"！" : L"";
        default: return L"";
    }
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) return CallNextHookEx(gHook, code, wParam, lParam);
    const KBDLLHOOKSTRUCT* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    DWORD vk = key->vkCode;
    bool keyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    if (keyUp) {
        if (gEnabled && gSettings.singleShiftToggle && vk == VK_SHIFT) {
            if (gShiftDown && !gShiftUsed) ToggleLanguageMode();
            gShiftDown = false;
        }
        return CallNextHookEx(gHook, code, wParam, lParam);
    }
    if (!gEnabled || (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN)) return CallNextHookEx(gHook, code, wParam, lParam);
    if (vk == VK_SHIFT) {
        if (gSettings.singleShiftToggle) {
            gShiftDown = true;
            gShiftUsed = false;
        }
        return CallNextHookEx(gHook, code, wParam, lParam);
    }
    if (gShiftDown) gShiftUsed = true;
    bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
    if ((control && alt && vk == 'G') || control || alt || win) return CallNextHookEx(gHook, code, wParam, lParam);
    if (gEnglishMode) return CallNextHookEx(gHook, code, wParam, lParam);
    if (!gComposition.empty() && !gCandidates.empty()) {
        if (vk == VK_PRIOR || vk == VK_UP || vk == VK_OEM_COMMA || vk == VK_OEM_MINUS) { MoveCandidatePage(-1); return 1; }
        if (vk == VK_NEXT || vk == VK_DOWN || vk == VK_OEM_PERIOD || vk == VK_OEM_PLUS) { MoveCandidatePage(1); return 1; }
        if (vk == VK_LEFT) { MoveCandidateFocus(-1); return 1; }
        if (vk == VK_RIGHT) { MoveCandidateFocus(1); return 1; }
    }
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    std::wstring punctuation = ChinesePunctuation(vk, shift);
    if (!punctuation.empty()) {
        if (!gComposition.empty()) ClearComposition();
        PasteText(punctuation);
        return 1;
    }
    if (vk >= 'A' && vk <= 'Z') {
        if (gComposition.size() >= MAX_COMPOSITION_CHARS) {
            gStatus = L"拼音過長，請先確認或按 Esc 取消";
            ResizeAndShowCandidate();
            return 1;
        }
        gComposition.push_back(static_cast<wchar_t>(towlower(static_cast<wchar_t>(vk))));
        gCandidates.clear();
        gStatus.clear();
        gFuzzyHint.clear();
        gCandidatePage = 0;
        gCandidateFocus = 0;
        ResizeAndShowCandidate();
        RequestCandidates();
        return 1;
    }
    if (vk == VK_BACK) {
        if (gComposition.empty()) return CallNextHookEx(gHook, code, wParam, lParam);
        gComposition.pop_back();
        gCandidates.clear();
        gStatus.clear();
        gFuzzyHint.clear();
        gCandidatePage = 0;
        gCandidateFocus = 0;
        if (gComposition.empty()) ClearComposition(); else { ResizeAndShowCandidate(); RequestCandidates(); }
        return 1;
    }
    if (vk == VK_ESCAPE) {
        if (!gComposition.empty()) { ClearComposition(); return 1; }
        return CallNextHookEx(gHook, code, wParam, lParam);
    }
    if (vk == VK_SPACE || vk == VK_RETURN) {
        if (!gCandidates.empty()) { SelectCandidate(CurrentPageStart() + gCandidateFocus); return 1; }
        if (!gComposition.empty()) return 1;
        return CallNextHookEx(gHook, code, wParam, lParam);
    }
    if (vk >= '1' && vk <= '9') {
        if (!gCandidates.empty()) {
            size_t index = CurrentPageStart() + static_cast<size_t>(vk - '1');
            if (index < gCandidates.size() && static_cast<size_t>(vk - '1') < CurrentPageSize()) SelectCandidate(index);
            return 1;
        }
        if (!gComposition.empty()) return 1;
    }
    return CallNextHookEx(gHook, code, wParam, lParam);
}

void ShowTrayMenu() {
    POINT point{}; GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, gEnabled ? L"關閉粵語輸入" : L"開啟粵語輸入");
    AppendMenuW(menu, MF_STRING, 2, gEnglishMode ? L"切換至中文模式（Shift）" : L"切換至英文模式（Shift）");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"開啟自訂詞庫");
    AppendMenuW(menu, MF_STRING, 4, L"開啟設定檔");
    AppendMenuW(menu, MF_STRING, 5, L"清除本機選字歷史");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 6, L"結束程式");
    SetForegroundWindow(gMain);
    int choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, point.x, point.y, 0, gMain, nullptr);
    DestroyMenu(menu);
    if (choice == 1) ToggleInput();
    if (choice == 2) ToggleLanguageMode();
    if (choice == 3) { EnsureDictionaryFile(); ShellExecuteW(nullptr, L"open", SystemNotepadPath().c_str(), DataFilePath(L"custom_dictionary.tsv").c_str(), nullptr, SW_SHOWNORMAL); }
    if (choice == 4) { EnsureSettingsFile(); ShellExecuteW(nullptr, L"open", SystemNotepadPath().c_str(), DataFilePath(L"settings.ini").c_str(), nullptr, SW_SHOWNORMAL); }
    if (choice == 5) { ClearHistoryFile(); gStatus = L"已清除本機選字歷史"; ResizeAndShowCandidate(); SetTimer(gMain, TIMER_STATUS, 1200, nullptr); }
    if (choice == 6) PostMessage(gMain, WM_CLOSE, 0, 0);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_HOTKEY:
            if (wParam == HOTKEY_ID) ToggleInput();
            return 0;
        case WM_CANDIDATES_READY: {
            CandidateResult* result = reinterpret_cast<CandidateResult*>(lParam);
            if (result && gEnabled && result->composition == gComposition) {
                gCandidates = result->candidates;
                gFuzzyHint = result->fuzzyHint;
                gCandidatePage = 0;
                gCandidateFocus = 0;
                gStatus = result->error;
                ResizeAndShowCandidate();
            }
            delete result;
            return 0;
        }
        case WM_TIMER:
            if (wParam == TIMER_STATUS) { KillTimer(hwnd, TIMER_STATUS); if (gComposition.empty()) { gStatus.clear(); ShowWindow(gCandidateWindow, SW_HIDE); } }
            if (wParam == TIMER_REQUEST) {
                KillTimer(hwnd, TIMER_REQUEST);
                if (!gPendingComposition.empty() && gPendingComposition == gComposition) RequestCandidates();
            }
            if (wParam == TIMER_CLIPBOARD_RESTORE) RestoreClipboardIfUnchanged();
            return 0;
        case WM_TRAY:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowTrayMenu();
            if (lParam == WM_LBUTTONUP) ToggleInput();
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY: {
            RestoreClipboardIfUnchanged();
            UnregisterHotKey(hwnd, HOTKEY_ID);
            if (gHook) UnhookWindowsHookEx(gHook);
            NOTIFYICONDATAW data{}; data.cbSize = sizeof(data); data.hWnd = hwnd; data.uID = TRAY_ID;
            Shell_NotifyIconW(NIM_DELETE, &data);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    OleInitialize(nullptr);
    gInstance = instance;
    const wchar_t* mainClass = L"CantoCandidateMain";
    const wchar_t* candidateClass = L"CantoCandidatePopup";
    WNDCLASSW mainWindow{};
    mainWindow.hInstance = instance;
    mainWindow.lpszClassName = mainClass;
    mainWindow.lpfnWndProc = MainWindowProc;
    RegisterClassW(&mainWindow);
    WNDCLASSW popupWindow{};
    popupWindow.hInstance = instance;
    popupWindow.lpszClassName = candidateClass;
    popupWindow.lpfnWndProc = CandidateWindowProc;
    popupWindow.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&popupWindow);
    EnsureDictionaryFile();
    LoadSettings();
    gMain = CreateWindowExW(0, mainClass, L"CantoCandidate", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    gCandidateWindow = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, candidateClass, L"", WS_POPUP, 0, 0, 0, 0, gMain, nullptr, instance, nullptr);
    RegisterHotKey(gMain, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'G');
    gHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, instance, 0);
    NOTIFYICONDATAW tray{};
    tray.cbSize = sizeof(tray); tray.hWnd = gMain; tray.uID = TRAY_ID;
    tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; tray.uCallbackMessage = WM_TRAY;
    tray.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(tray.szTip, L"粵語候選字：關閉（Ctrl+Alt+G 開啟）");
    Shell_NotifyIconW(NIM_ADD, &tray);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) { TranslateMessage(&message); DispatchMessageW(&message); }
    OleUninitialize();
    return 0;
}
