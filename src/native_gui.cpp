#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cderr.h>
#include <shellapi.h>
#include <shlobj.h>

#include "encoder_api.hpp"
#include "codec_profiles.hpp"
#include "gui_i18n.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;
using atracdenc_aal_gui::tr;
constexpr wchar_t kClassName[] = L"AtracDEncAalEncoderWindow";
constexpr UINT kWorkerMessage = WM_APP + 41;
constexpr COLORREF kBackground = RGB(246, 248, 251);
constexpr COLORREF kInk = RGB(24, 34, 52);
constexpr COLORREF kMuted = RGB(91, 105, 126);

enum ControlId {
    AddFiles = 101, RemoveFiles, ClearQueue, FileList, OutputFolder,
    BrowseFolder, StartEncoding, CancelWork, RunSelfTest, OpenFolder, LogBox, ProfileChoice
};
enum class RowState { Pending, Working, Done, Failed, Cancelled };
struct QueueRow {
    fs::path input;
    fs::path output;
    RowState state = RowState::Pending;
    int progress = 0;
    explicit QueueRow(fs::path path) : input(std::move(path)) {}
};
enum class EventKind { StartFile, Progress, FileDone, FileError, FileCancelled,
                       Log, Finished };
struct WorkerEvent {
    EventKind kind;
    size_t row = size_t(-1);
    int percent = 0;
    std::wstring text;
    fs::path output;
    WorkerEvent(EventKind event_kind, size_t index = size_t(-1), int progress = 0,
                std::wstring message = {}, fs::path target = {})
        : kind(event_kind), row(index), percent(progress), text(std::move(message)),
          output(std::move(target)) {}
};
struct App {
    HWND window = nullptr;
    HWND title = nullptr, subtitle = nullptr, note = nullptr;
    HWND add = nullptr, remove = nullptr, clear = nullptr, drop_hint = nullptr;
    HWND profile = nullptr;
    HWND list = nullptr, output_label = nullptr, output = nullptr, browse = nullptr;
    HWND phase = nullptr, percent = nullptr, progress = nullptr;
    HWND start = nullptr, cancel = nullptr, self_test = nullptr, open = nullptr;
    HWND queue_summary = nullptr, log_label = nullptr, log = nullptr, footer = nullptr;
    HFONT body_font = nullptr, title_font = nullptr, small_font = nullptr;
    HBRUSH background = nullptr;
    unsigned dpi = 96;
    bool busy = false, closing = false, output_chosen = false;
    bool setting_output = false, first_input_default = false;
    std::vector<QueueRow> rows;
    std::vector<fs::path> startup_paths;
    std::atomic_bool cancelled{false};
    std::thread worker;
};

int px(const App& app, int value) { return MulDiv(value, int(app.dpi), 96); }

std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    UINT encoding = CP_UTF8;
    int count = MultiByteToWideChar(encoding, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!count) {
        encoding = CP_ACP;
        count = MultiByteToWideChar(encoding, 0, text.data(), int(text.size()), nullptr, 0);
    }
    std::wstring result(size_t(count), L'\0');
    if (count) MultiByteToWideChar(encoding, 0, text.data(), int(text.size()), result.data(), count);
    return result;
}

std::wstring control_text(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(size_t(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(size_t(length));
    return text;
}

void append_log(App& app, const std::wstring& message) {
    SYSTEMTIME clock{};
    GetLocalTime(&clock);
    wchar_t prefix[32];
    swprintf(prefix, 32, L"%02u:%02u:%02u  ", clock.wHour, clock.wMinute, clock.wSecond);
    std::wstring line = prefix + message + L"\r\n";
    // Keep the native edit control responsive during long batches.
    int length = GetWindowTextLengthW(app.log);
    if (length > 350000) {
        SendMessageW(app.log, EM_SETSEL, 0, 100000);
        SendMessageW(app.log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
    }
    SendMessageW(app.log, EM_SETSEL, WPARAM(-1), LPARAM(-1));
    SendMessageW(app.log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(app.log, EM_SCROLLCARET, 0, 0);
}

std::wstring state_name(RowState state) {
    switch (state) {
    case RowState::Pending: return tr(L"等待");
    case RowState::Working: return tr(L"编码中");
    case RowState::Done: return tr(L"已完成");
    case RowState::Failed: return tr(L"失败");
    case RowState::Cancelled: return tr(L"已取消");
    }
    return {};
}

void refresh_row(App& app, size_t index) {
    if (index >= app.rows.size()) return;
    auto& row = app.rows[index];
    std::wstring status = state_name(row.state);
    std::wstring progress = std::to_wstring(row.progress) + L"%";
    std::wstring output = row.output.empty() ? L"—" : row.output.filename().wstring();
    ListView_SetItemText(app.list, int(index), 1, status.data());
    ListView_SetItemText(app.list, int(index), 2, progress.data());
    ListView_SetItemText(app.list, int(index), 3, output.data());
}

void update_controls(App& app) {
    size_t completed = 0, waiting = 0;
    for (const auto& row : app.rows) {
        if (row.state == RowState::Done) ++completed;
        else ++waiting;
    }
    const std::wstring summary = std::to_wstring(app.rows.size()) + tr(L" 个文件  ·  已完成 ") + std::to_wstring(completed);
    SetWindowTextW(app.queue_summary, summary.c_str());
    for (HWND control : {app.add, app.remove, app.clear, app.profile, app.list, app.output, app.browse, app.self_test})
        EnableWindow(control, !app.busy && !app.closing);
    EnableWindow(app.start, !app.busy && !app.closing && waiting > 0);
    EnableWindow(app.cancel, app.busy && !app.cancelled.load());
    EnableWindow(app.open, !app.closing);
    EnableWindow(app.remove, !app.busy && !app.closing && !app.rows.empty());
    EnableWindow(app.clear, !app.busy && !app.closing && !app.rows.empty());
}

void set_output(App& app, const fs::path& path) {
    app.setting_output = true;
    SetWindowTextW(app.output, path.c_str());
    app.setting_output = false;
}

uint16_t le16(const unsigned char* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t le32(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

// Early user-facing validation. The engine repeats all format checks before
// encoding; this does not replace the engine's authoritative validation.
std::wstring wave_problem(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return tr(L"不是可读取的音频文件");
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
    if (extension != L".wav") return tr(L"请选择 PCM WAV 文件");
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 12> riff{};
    if (!input.read(reinterpret_cast<char*>(riff.data()), riff.size())) return tr(L"WAV 文件头不完整");
    if (std::memcmp(riff.data(), "RIFF", 4) || std::memcmp(riff.data() + 8, "WAVE", 4)) return tr(L"需要标准 RIFF / PCM WAV 文件");
    for (unsigned chunks = 0; chunks < 4096; ++chunks) {
        std::array<unsigned char, 8> chunk{};
        if (!input.read(reinterpret_cast<char*>(chunk.data()), chunk.size())) break;
        const uint32_t size = le32(chunk.data() + 4);
        if (!std::memcmp(chunk.data(), "fmt ", 4)) {
            std::array<unsigned char, 16> format{};
            if (size < 16 || !input.read(reinterpret_cast<char*>(format.data()), format.size())) return tr(L"WAV 格式信息不完整");
            if (le16(format.data()) != 1 || le16(format.data() + 2) != 2 ||
                le32(format.data() + 4) != 44100 || le16(format.data() + 14) != 16 ||
                le16(format.data() + 12) != 4 || le32(format.data() + 8) != 176400)
                return tr(L"仅支持 44.1 kHz、16 位、双声道 PCM WAV");
            return {};
        }
        input.seekg(std::streamoff(size) + (size & 1u), std::ios::cur);
        if (!input) break;
    }
    return tr(L"未找到有效的 PCM 格式信息");
}

bool same_path(const fs::path& left, const fs::path& right) {
    const std::wstring a = left.wstring(), b = right.wstring();
    return CompareStringOrdinal(a.c_str(), int(a.size()), b.c_str(), int(b.size()), TRUE) == CSTR_EQUAL;
}

void add_paths(App& app, const std::vector<fs::path>& paths) {
    if (app.busy || app.closing) return;
    unsigned added = 0, skipped = 0;
    for (const auto& candidate : paths) {
        std::error_code ec;
        fs::path path = fs::weakly_canonical(candidate, ec);
        if (ec) path = candidate;
        const auto problem = wave_problem(path);
        if (!problem.empty()) {
            append_log(app, tr(L"已跳过：") + path.filename().wstring() + L" — " + problem);
            ++skipped;
            continue;
        }
        bool duplicate = false;
        for (const auto& row : app.rows) if (same_path(row.input, path)) { duplicate = true; break; }
        if (duplicate) { ++skipped; continue; }
        if (!app.first_input_default) {
            if (!app.output_chosen) set_output(app, path.parent_path() / L"AAL");
            app.first_input_default = true;
        }
        const int index = int(app.rows.size());
        app.rows.emplace_back(path);
        std::wstring filename = path.filename().wstring();
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.pszText = filename.data();
        ListView_InsertItem(app.list, &item);
        refresh_row(app, size_t(index));
        ++added;
    }
    if (added) append_log(app, tr(L"已添加 ") + std::to_wstring(added) + tr(L" 个文件。可继续添加或开始编码。"));
    else if (skipped && paths.size() == 1) SetWindowTextW(app.phase, tr(L"文件未加入队列，请查看下方日志"));
    update_controls(app);
}

void pick_files(App& app) {
    std::vector<wchar_t> names(65536, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = tr(L"PCM WAV 音频 (*.wav)\0*.wav\0所有文件 (*.*)\0*.*\0\0");
    dialog.lpstrFile = names.data();
    dialog.nMaxFile = DWORD(names.size());
    dialog.lpstrTitle = tr(L"添加 44.1 kHz / 16 位 / 双声道 PCM WAV");
    dialog.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) {
        if (CommDlgExtendedError() == FNERR_BUFFERTOOSMALL)
            append_log(app, tr(L"本次选择的文件过多，请分批添加。"));
        return;
    }
    std::vector<fs::path> paths;
    fs::path first(names.data());
    const wchar_t* next = names.data() + first.native().size() + 1;
    if (!*next) paths.push_back(first);
    else for (; *next; next += std::wcslen(next) + 1) paths.push_back(first / next);
    add_paths(app, paths);
}

int CALLBACK browse_callback(HWND window, UINT message, LPARAM, LPARAM data) {
    if (message == BFFM_INITIALIZED && data)
        SendMessageW(window, BFFM_SETSELECTIONW, TRUE, data);
    return 0;
}

void pick_output(App& app) {
    std::wstring initial = control_text(app.output);
    BROWSEINFOW dialog{};
    dialog.hwndOwner = app.window;
    dialog.lpszTitle = tr(L"选择 AAL 文件和校验报告的保存文件夹");
    dialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    dialog.lpfn = browse_callback;
    dialog.lParam = reinterpret_cast<LPARAM>(initial.c_str());
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&dialog);
    if (!item) return;
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(item, path)) {
        set_output(app, path);
        app.output_chosen = true;
    }
    CoTaskMemFree(item);
}

void remove_selected(App& app) {
    if (app.busy) return;
    for (int index = int(app.rows.size()) - 1; index >= 0; --index) {
        if (ListView_GetItemState(app.list, index, LVIS_SELECTED) & LVIS_SELECTED) {
            ListView_DeleteItem(app.list, index);
            app.rows.erase(app.rows.begin() + index);
        }
    }
    update_controls(app);
}

void post_event(HWND window, WorkerEvent event) {
    auto owned = std::make_unique<WorkerEvent>(std::move(event));
    if (PostMessageW(window, kWorkerMessage, 0, reinterpret_cast<LPARAM>(owned.get()))) owned.release();
}

fs::path vacant_output(const fs::path& folder, const fs::path& input) {
    std::wstring stem = input.stem().wstring();
    if (stem.empty()) stem = tr(L"音频");
    for (unsigned suffix = 1; suffix < 1000000; ++suffix) {
        fs::path output = folder / (stem + (suffix == 1 ? L"" : L" (" + std::to_wstring(suffix) + L")") + L".oma");
        fs::path report = output;
        report += L".json";
        if (!fs::exists(output) && !fs::exists(report)) return output;
    }
    throw std::runtime_error("No unused output filename is available.");
}

std::wstring megabytes(uint64_t bytes) {
    std::wostringstream text;
    text << std::fixed << std::setprecision(1) << double(bytes) / 1048576.0 << L" MB";
    return text.str();
}

void begin_work(App& app, bool test_only) {
    if (app.busy || app.closing) return;
    const LRESULT selected=SendMessageW(app.profile,CB_GETCURSEL,0,0);
    if(selected<0||size_t(selected)>=independent_aal::kOfficialProfiles.size())throw std::runtime_error("Select a valid AAL core profile.");
    independent_aal::EncodeOptions options;options.profile_id=independent_aal::kOfficialProfiles[size_t(selected)].id;
    struct Job { size_t row; fs::path input; };
    std::vector<Job> jobs;
    if (!test_only) for (size_t i = 0; i < app.rows.size(); ++i)
        if (app.rows[i].state != RowState::Done) jobs.push_back({i, app.rows[i].input});
    if (!test_only && jobs.empty()) return;
    const std::wstring folder_text = control_text(app.output);
    if (!test_only && folder_text.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        append_log(app, tr(L"请先设置输出文件夹。"));
        SetFocus(app.output);
        return;
    }
    fs::path folder(folder_text);
    if (!test_only) for (const auto& job : jobs) {
        app.rows[job.row].state = RowState::Pending;
        app.rows[job.row].progress = 0;
        app.rows[job.row].output.clear();
        refresh_row(app, job.row);
    }
    app.cancelled.store(false);
    app.busy = true;
    SetWindowTextW(app.phase, test_only ? tr(L"正在进行内置自检…") : tr(L"正在准备编码…"));
    SetWindowTextW(app.percent, L"0%");
    SendMessageW(app.progress, PBM_SETPOS, 0, 0);
    append_log(app, test_only ? tr(L"开始内置自检：检查独立编码、解码与 PCM 一致性。") : tr(L"开始编码。每个文件都会完成无损一致性校验后保存。"));
    append_log(app,tr(L"本次核心配置：")+widen(options.profile_id));
    update_controls(app);
    HWND window = app.window;
    try {
        app.worker = std::thread([window, &app, jobs = std::move(jobs), folder, test_only, options]() {
            unsigned completed = 0, failed = 0;
            try {
                if (!test_only) fs::create_directories(folder);
                auto progress_for = [&](size_t row) {
                    return [window, row, last_percent = -1, last_text = std::wstring(),
                            last_time = std::chrono::steady_clock::now()](int percent, const std::wstring& text) mutable {
                        percent = std::max(0, std::min(100, percent));
                        const auto now = std::chrono::steady_clock::now();
                        if (percent != last_percent || text != last_text || now - last_time > std::chrono::milliseconds(100)) {
                            post_event(window, {EventKind::Progress, row, percent, atracdenc_aal_gui::progress_text(text)});
                            last_percent = percent; last_text = text; last_time = now;
                        }
                    };
                };
                if (test_only) {
                    const auto checksum = independent_aal::self_test(progress_for(size_t(-1)), &app.cancelled,options);
                    post_event(window, {EventKind::Log, size_t(-1), 100, tr(L"自检通过：编码、解码及 PCM 一致性检查完成。")});
                    post_event(window, {EventKind::Log, size_t(-1), 100, tr(L"自检 SHA-256：") + widen(checksum)});
                    ++completed;
                } else for (size_t position = 0; position < jobs.size(); ++position) {
                    if (app.cancelled.load()) break;
                    const auto& job = jobs[position];
                    try {
                        const fs::path output = vacant_output(folder, job.input);
                        post_event(window, {EventKind::StartFile, job.row, 0,
                            tr(L"正在处理 ") + std::to_wstring(position + 1) + L" / " + std::to_wstring(jobs.size()) + tr(L"：") + job.input.filename().wstring(), output});
                        const auto result = independent_aal::encode_file(job.input, output, progress_for(job.row), &app.cancelled,options);
                        post_event(window, {EventKind::FileDone, job.row, 100,
                            tr(L"完成：") + job.input.filename().wstring() + L" → " + result.output_path.filename().wstring() +
                            (result.verified ? tr(L"（无损校验通过，") : tr(L"（")) + megabytes(result.output_bytes) + tr(L"）"), result.output_path});
                        ++completed;
                    } catch (const std::exception& error) {
                        if (app.cancelled.load() || std::string(error.what()) == "Cancelled") {
                            app.cancelled.store(true);
                            post_event(window, {EventKind::FileCancelled, job.row, 0, tr(L"已取消：") + job.input.filename().wstring()});
                            break;
                        }
                        ++failed;
                        post_event(window, {EventKind::FileError, job.row, 0,
                            tr(L"编码失败：") + job.input.filename().wstring() + L" — " + widen(error.what())});
                    }
                }
            } catch (const std::exception& error) {
                if (!app.cancelled.load() && std::string(error.what()) != "Cancelled") {
                    ++failed;
                    post_event(window, {EventKind::Log, size_t(-1), 0,
                        std::wstring(test_only ? tr(L"自检失败：") : tr(L"无法开始编码：")) + widen(error.what())});
                } else app.cancelled.store(true);
            } catch (...) {
                ++failed;
                post_event(window, {EventKind::Log, size_t(-1), 0, tr(L"处理失败，请检查音频文件和输出目录后重试。")});
            }
            std::wstring message;
            if (app.cancelled.load()) message = tr(L"已取消。已完成的文件已保留，可再次开始处理其余文件。");
            else if (test_only) message = failed ? tr(L"自检未通过，请查看日志。") : tr(L"内置自检通过。");
            else message = tr(L"本次处理结束：成功 ") + std::to_wstring(completed) + tr(L" 个，失败 ") + std::to_wstring(failed) + tr(L" 个。");
            post_event(window, {EventKind::Finished, size_t(-1), failed ? 0 : 100, std::move(message)});
        });
    } catch (const std::exception& error) {
        app.busy = false;
        append_log(app, tr(L"无法启动后台任务：") + widen(error.what()));
        update_controls(app);
    }
}

void receive_event(App& app, WorkerEvent& event) {
    switch (event.kind) {
    case EventKind::StartFile:
        if (event.row < app.rows.size()) {
            auto& row = app.rows[event.row]; row.state = RowState::Working; row.progress = 0; row.output = event.output;
            refresh_row(app, event.row);
            ListView_EnsureVisible(app.list, int(event.row), FALSE);
        }
        SetWindowTextW(app.phase, event.text.c_str());
        SetWindowTextW(app.percent, L"0%");
        SendMessageW(app.progress, PBM_SETPOS, 0, 0);
        break;
    case EventKind::Progress: {
        if (event.row < app.rows.size()) {
            app.rows[event.row].progress = event.percent;
            refresh_row(app, event.row);
        }
        const std::wstring text = event.row < app.rows.size()
            ? app.rows[event.row].input.filename().wstring() + L" · " + event.text : event.text;
        if (!app.cancelled.load()) SetWindowTextW(app.phase, text.c_str());
        const std::wstring percent = std::to_wstring(event.percent) + L"%";
        SetWindowTextW(app.percent, percent.c_str());
        SendMessageW(app.progress, PBM_SETPOS, event.percent, 0);
        break;
    }
    case EventKind::FileDone:
    case EventKind::FileError:
    case EventKind::FileCancelled:
        if (event.row < app.rows.size()) {
            auto& row = app.rows[event.row];
            row.state = event.kind == EventKind::FileDone ? RowState::Done :
                        event.kind == EventKind::FileError ? RowState::Failed : RowState::Cancelled;
            if (event.kind == EventKind::FileDone) { row.progress = 100; row.output = event.output; }
            refresh_row(app, event.row);
        }
        append_log(app, event.text);
        update_controls(app);
        break;
    case EventKind::Log: append_log(app, event.text); break;
    case EventKind::Finished:
        if (app.worker.joinable()) app.worker.join();
        app.busy = false;
        if (!app.cancelled.load() && event.percent == 100) {
            SetWindowTextW(app.percent, L"100%");
            SendMessageW(app.progress, PBM_SETPOS, 100, 0);
        }
        SetWindowTextW(app.phase, event.text.c_str());
        append_log(app, event.text);
        update_controls(app);
        if (app.closing) DestroyWindow(app.window);
        break;
    }
}

HWND make_control(App& app, const wchar_t* class_name, const wchar_t* text,
                  DWORD style, int id = 0, DWORD extended = 0) {
    HWND control = CreateWindowExW(extended, class_name, text,
        WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, app.window,
        reinterpret_cast<HMENU>(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
    if (!control) throw std::runtime_error("Cannot create a window control.");
    return control;
}

unsigned window_dpi(HWND window) {
    using Function = UINT (WINAPI*)(HWND);
    const auto address = GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    Function get_dpi = nullptr;
    static_assert(sizeof get_dpi == sizeof address);
    std::memcpy(&get_dpi, &address, sizeof get_dpi);
    if (get_dpi) return get_dpi(window);
    HDC dc = GetDC(window);
    const unsigned dpi = dc ? unsigned(GetDeviceCaps(dc, LOGPIXELSX)) : 96;
    if (dc) ReleaseDC(window, dc);
    return dpi ? dpi : 96;
}

void update_fonts(App& app) {
    if (app.body_font) DeleteObject(app.body_font);
    if (app.title_font) DeleteObject(app.title_font);
    if (app.small_font) DeleteObject(app.small_font);
    auto font = [&](int pixels, int weight) {
        return CreateFontW(-px(app, pixels), 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, atracdenc_aal_gui::language == atracdenc_aal_gui::Language::Chinese ? L"Microsoft YaHei UI" : L"Segoe UI");
    };
    app.body_font = font(14, FW_NORMAL);
    app.title_font = font(25, FW_SEMIBOLD);
    app.small_font = font(12, FW_NORMAL);
    for (HWND control : {app.title, app.subtitle, app.note, app.add, app.remove,
            app.clear, app.drop_hint, app.profile, app.list, app.output_label, app.output,
            app.browse, app.phase, app.percent, app.start, app.cancel, app.self_test,
            app.open, app.queue_summary, app.log_label, app.log, app.footer})
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(app.body_font), TRUE);
    SendMessageW(app.title, WM_SETFONT, reinterpret_cast<WPARAM>(app.title_font), TRUE);
    for (HWND control : {app.drop_hint, app.note, app.footer})
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(app.small_font), TRUE);
}

void layout(App& app) {
    RECT client{}; GetClientRect(app.window, &client);
    const int width = MulDiv(client.right, 96, int(app.dpi));
    const int height = MulDiv(client.bottom, 96, int(app.dpi));
    const int margin = 24, available = width - margin * 2;
    const int log_height = std::max(92, std::min(140, height - 620));
    const int log_top = height - 37 - log_height;
    const int log_label = log_top - 24, action = log_label - 48;
    const int bar = action - 24, phase = bar - 27, output = phase - 44;
    const int output_label = output - 25;
    auto place = [&](HWND control, int x, int y, int w, int h) {
        MoveWindow(control, px(app,x), px(app,y), px(app,w), px(app,h), TRUE);
    };
    place(app.title, margin, 20, available, 38);
    place(app.subtitle, margin, 65, available, 22);
    place(app.note, margin, 92, available, 20);
    place(app.add, margin, 122, 130, 30);
    place(app.remove, margin + 142, 122, 104, 30);
    place(app.clear, margin + 258, 122, 104, 30);
    place(app.drop_hint, margin + 385, 129, 64, 20);
    place(app.profile, margin + 454, 122, available - 454, 300);
    place(app.list, margin, 164, available, std::max(130, output_label - 177));
    place(app.output_label, margin, output_label, available, 22);
    place(app.output, margin, output, available - 132, 32);
    place(app.browse, width - margin - 120, output, 120, 32);
    place(app.phase, margin, phase, available - 62, 22);
    place(app.percent, width - margin - 58, phase, 58, 22);
    place(app.progress, margin, bar, available, 12);
    place(app.start, margin, action, 130, 34);
    place(app.cancel, margin + 142, action, 88, 34);
    place(app.self_test, margin + 242, action, 110, 34);
    place(app.open, margin + 364, action, 150, 34);
    place(app.queue_summary, margin + 535, action + 7, available - 535, 22);
    place(app.log_label, margin, log_label, available, 22);
    place(app.log, margin, log_top, available, log_height);
    place(app.footer, margin, height - 24, available, 18);
    const int status = px(app, 98), percent_width = px(app, 75);
    const int output_width = px(app, 220);
    ListView_SetColumnWidth(app.list, 0, std::max(px(app,180), px(app,available)-status-percent_width-output_width-px(app,5)));
    ListView_SetColumnWidth(app.list, 1, status);
    ListView_SetColumnWidth(app.list, 2, percent_width);
    ListView_SetColumnWidth(app.list, 3, output_width);
}

void initialize(App& app) {
    app.background = CreateSolidBrush(kBackground);
    app.dpi = window_dpi(app.window);
    app.title = make_control(app, L"STATIC", L"AtracDEnc-AAL", SS_LEFT);
    app.subtitle = make_control(app, L"STATIC", tr(L"简易测试图形界面  ·  基于 AtracDEnc 的 AAL 编码器"), SS_LEFT);
    app.note = make_control(app, L"STATIC", tr(L"输入要求：44.1 kHz / 16-bit / 双声道 PCM WAV。编码完成前自动逐样本校验。"), SS_LEFT);
    app.add = make_control(app, L"BUTTON", tr(L"添加 WAV(&A)…"), BS_PUSHBUTTON | WS_TABSTOP, AddFiles);
    app.remove = make_control(app, L"BUTTON", tr(L"移除选中"), BS_PUSHBUTTON | WS_TABSTOP, RemoveFiles);
    app.clear = make_control(app, L"BUTTON", tr(L"清空队列"), BS_PUSHBUTTON | WS_TABSTOP, ClearQueue);
    app.drop_hint = make_control(app, L"STATIC", tr(L"编码核心"), SS_LEFT);
    app.profile = make_control(app,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,ProfileChoice);
    for(const auto& profile:independent_aal::kOfficialProfiles) {
        const std::wstring title=widen(profile.container_name)+L" / "+(profile.codec==independent_aal::CoreCodec::Atrac3plus?L"ATRAC3plus ":L"ATRAC3 ")+std::to_wstring(profile.nominal_bitrate/1000)+L" kbps";
        SendMessageW(app.profile,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(title.c_str()));
    }
    SendMessageW(app.profile,CB_SETCURSEL,9,0);
    app.list = make_control(app, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP | WS_BORDER, FileList);
    ListView_SetExtendedListViewStyle(app.list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ListView_SetBkColor(app.list, RGB(255,255,255));
    ListView_SetTextBkColor(app.list, RGB(255,255,255));
    ListView_SetTextColor(app.list, kInk);
    const wchar_t* headings[] = {tr(L"文件名称"), tr(L"状态"), tr(L"进度"), tr(L"输出文件")};
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        column.fmt = LVCFMT_LEFT;
        column.cx = 120;
        column.pszText = const_cast<wchar_t*>(headings[i]);
        ListView_InsertColumn(app.list, i, &column);
    }
    app.output_label = make_control(app, L"STATIC", tr(L"输出文件夹（重名自动加序号，同时保存校验报告）"), SS_LEFT);
    app.output = make_control(app, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, OutputFolder, WS_EX_CLIENTEDGE);
    SendMessageW(app.output, EM_SETLIMITTEXT, 30000, 0);
    app.browse = make_control(app, L"BUTTON", tr(L"选择文件夹…"), BS_PUSHBUTTON | WS_TABSTOP, BrowseFolder);
    app.phase = make_control(app, L"STATIC", tr(L"准备就绪：添加 44.1 kHz、16 位、双声道 PCM WAV"), SS_LEFT | SS_ENDELLIPSIS);
    app.percent = make_control(app, L"STATIC", L"0%", SS_RIGHT);
    app.progress = make_control(app, PROGRESS_CLASSW, L"", PBS_SMOOTH);
    SendMessageW(app.progress, PBM_SETRANGE32, 0, 100);
    SendMessageW(app.progress, PBM_SETBARCOLOR, 0, RGB(35,103,206));
    app.start = make_control(app, L"BUTTON", tr(L"开始编码(&S)"), BS_DEFPUSHBUTTON | WS_TABSTOP, StartEncoding);
    app.cancel = make_control(app, L"BUTTON", tr(L"取消"), BS_PUSHBUTTON | WS_TABSTOP, CancelWork);
    app.self_test = make_control(app, L"BUTTON", tr(L"内置自检"), BS_PUSHBUTTON | WS_TABSTOP, RunSelfTest);
    app.open = make_control(app, L"BUTTON", tr(L"打开输出文件夹"), BS_PUSHBUTTON | WS_TABSTOP, OpenFolder);
    app.queue_summary = make_control(app, L"STATIC", L"", SS_RIGHT);
    app.log_label = make_control(app, L"STATIC", tr(L"处理日志"), SS_LEFT);
    app.log = make_control(app, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, LogBox, WS_EX_CLIENTEDGE);
    SendMessageW(app.log, EM_SETLIMITTEXT, 500000, 0);
    app.footer = make_control(app, L"STATIC", tr(L"实验版 0.2.0  ·  Sony X1050 / X1060 等老款 Walkman 的实机兼容性待验证"), SS_LEFT);
    wchar_t executable[32768];
    DWORD length = GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable)));
    fs::path program = length && length < std::size(executable) ? fs::path(std::wstring(executable,length)).parent_path() : fs::current_path();
    set_output(app, program / L"Converted");
    update_fonts(app);
    layout(app);
    update_controls(app);
    DragAcceptFiles(app.window, TRUE);
    append_log(app, tr(L"就绪。首次添加文件后，默认保存到首个音源目录下的 AAL 文件夹。"));
    if (!app.startup_paths.empty()) add_paths(app, app.startup_paths);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window,message,wparam,lparam);
    try {
        switch (message) {
        case WM_CREATE: initialize(*app); return 0;
        case WM_SIZE: if (app->list) layout(*app); return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize = {px(*app,900),px(*app,740)};
            return 0;
        }
        case WM_DPICHANGED: {
            app->dpi = HIWORD(wparam);
            update_fonts(*app);
            const RECT& bounds = *reinterpret_cast<RECT*>(lparam);
            SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
            layout(*app);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
            case AddFiles: if (!app->busy) pick_files(*app); return 0;
            case RemoveFiles: remove_selected(*app); return 0;
            case ClearQueue:
                if (!app->busy) {
                    app->rows.clear(); ListView_DeleteAllItems(app->list);
                    if (!app->output_chosen) app->first_input_default = false;
                    update_controls(*app);
                }
                return 0;
            case BrowseFolder: if (!app->busy) pick_output(*app); return 0;
            case OutputFolder:
                if (HIWORD(wparam) == EN_CHANGE && !app->setting_output) app->output_chosen = true;
                return 0;
            case IDOK:
            case StartEncoding: begin_work(*app,false); return 0;
            case RunSelfTest: begin_work(*app,true); return 0;
            case IDCANCEL:
            case CancelWork:
                if (app->busy && !app->cancelled.exchange(true)) {
                    SetWindowTextW(app->phase,tr(L"正在取消并清理，请稍候…"));
                    append_log(*app,tr(L"已请求取消，正在等待当前任务安全结束。"));
                    update_controls(*app);
                }
                return 0;
            case OpenFolder: {
                fs::path folder(control_text(app->output));
                std::error_code error;
                if (fs::is_directory(folder,error)) ShellExecuteW(window,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                else append_log(*app,tr(L"输出文件夹尚未创建，开始编码后将自动建立。"));
                return 0;
            }
            }
            break;
        case WM_NOTIFY: {
            auto* notification = reinterpret_cast<NMHDR*>(lparam);
            if (notification->idFrom == FileList && notification->code == LVN_KEYDOWN &&
                reinterpret_cast<NMLVKEYDOWN*>(lparam)->wVKey == VK_DELETE) remove_selected(*app);
            return 0;
        }
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wparam);
            std::vector<fs::path> paths;
            const UINT count = DragQueryFileW(drop,0xffffffffu,nullptr,0);
            for (UINT i = 0; i < count; ++i) {
                const UINT length = DragQueryFileW(drop,i,nullptr,0);
                std::wstring path(size_t(length)+1,L'\0');
                DragQueryFileW(drop,i,path.data(),length+1);
                path.resize(length); paths.emplace_back(path);
            }
            DragFinish(drop);
            if (app->busy) append_log(*app,tr(L"当前任务结束后，可继续拖入文件。"));
            else add_paths(*app,paths);
            return 0;
        }
        case kWorkerMessage: {
            std::unique_ptr<WorkerEvent> event(reinterpret_cast<WorkerEvent*>(lparam));
            if (event) receive_event(*app,*event);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            const HWND control = reinterpret_cast<HWND>(lparam);
            if (control == app->log) {
                SetBkColor(dc,RGB(255,255,255)); SetTextColor(dc,kInk);
                return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
            }
            SetBkColor(dc,kBackground);
            SetTextColor(dc,control == app->title ? kInk : kMuted);
            return reinterpret_cast<LRESULT>(app->background);
        }
        case WM_ERASEBKGND: {
            RECT client; GetClientRect(window,&client);
            FillRect(reinterpret_cast<HDC>(wparam),&client,app->background);
            return 1;
        }
        case WM_CLOSE:
            if (app->busy) {
                app->closing = true; app->cancelled.store(true);
                SetWindowTextW(app->phase,tr(L"正在取消并清理，完成后自动关闭…"));
                update_controls(*app);
            } else DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            DragAcceptFiles(window,FALSE);
            if (app->body_font) DeleteObject(app->body_font);
            if (app->title_font) DeleteObject(app->title_font);
            if (app->small_font) DeleteObject(app->small_font);
            if (app->background) DeleteObject(app->background);
            app->body_font = app->title_font = app->small_font = nullptr;
            app->background = nullptr;
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception& error) {
        if (app->log) append_log(*app,tr(L"操作未完成：") + widen(error.what()));
        else MessageBoxW(window,tr(L"窗口初始化失败。"),L"AtracDEnc-AAL",MB_OK|MB_ICONERROR);
        if (message == WM_CREATE) return -1;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    atracdenc_aal_gui::language = atracdenc_aal_gui::system_language();
    int argument_count = 0;
    LPWSTR* raw_arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    std::vector<std::wstring> arguments;
    if (raw_arguments) {
        for (int i = 1; i < argument_count; ++i) arguments.emplace_back(raw_arguments[i]);
        LocalFree(raw_arguments);
    }
    atracdenc_aal_gui::Startup startup;
    if (!atracdenc_aal_gui::parse_arguments(arguments, atracdenc_aal_gui::language, startup)) {
        MessageBoxW(nullptr, tr(L"语言参数无效。请使用 --lang auto、zh 或 en。"), L"AtracDEnc-AAL", MB_OK|MB_ICONERROR);
        return 4;
    }
    atracdenc_aal_gui::language = startup.language;
    using SetDpiContext = BOOL (WINAPI*)(HANDLE);
    const auto address = GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext");
    SetDpiContext set_dpi_context = nullptr;
    static_assert(sizeof set_dpi_context == sizeof address);
    std::memcpy(&set_dpi_context, &address, sizeof set_dpi_context);
    if (set_dpi_context) set_dpi_context(reinterpret_cast<HANDLE>(INT_PTR(-4)));
    else SetProcessDPIAware();
    const HRESULT com = OleInitialize(nullptr);
    INITCOMMONCONTROLSEX common{sizeof common,ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&common);
    App app;
    for (const auto& path : startup.paths) app.startup_paths.emplace_back(path);
    WNDCLASSEXW registration{};
    registration.cbSize = sizeof registration;
    registration.style = CS_HREDRAW | CS_VREDRAW;
    registration.lpfnWndProc = window_proc;
    registration.hInstance = instance;
    registration.hIcon = LoadIconW(nullptr,IDI_APPLICATION);
    registration.hIconSm = registration.hIcon;
    registration.hCursor = LoadCursorW(nullptr,IDC_ARROW);
    registration.lpszClassName = kClassName;
    if (!RegisterClassExW(&registration)) {
        if (SUCCEEDED(com)) OleUninitialize();
        return 1;
    }
    HDC dc = GetDC(nullptr);
    app.dpi = dc ? unsigned(GetDeviceCaps(dc,LOGPIXELSX)) : 96;
    if (dc) ReleaseDC(nullptr,dc);
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT,kClassName,tr(L"AtracDEnc-AAL · 简易测试图形界面 0.2.0"),
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,
        px(app,1040),px(app,820),nullptr,nullptr,instance,&app);
    if (!window) {
        if (SUCCEEDED(com)) OleUninitialize();
        return 2;
    }
    ShowWindow(window,show);
    UpdateWindow(window);
    MSG message{};
    int result;
    while ((result = int(GetMessageW(&message,nullptr,0,0))) > 0) {
        if (!IsDialogMessageW(window,&message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    app.cancelled.store(true);
    if (app.worker.joinable()) app.worker.join();
    if (SUCCEEDED(com)) OleUninitialize();
    return result == -1 ? 3 : int(message.wParam);
}
