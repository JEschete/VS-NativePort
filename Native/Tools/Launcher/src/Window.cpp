#include "Launcher/Window.hpp"

#include <Windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Platform/App.hpp"
#include "Platform/Paths.hpp"

namespace VS::Launcher {
namespace {
namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;
constexpr COLORREF kCanvas = RGB(15, 23, 35);
constexpr COLORREF kCard = RGB(25, 36, 51);
constexpr COLORREF kInput = RGB(12, 21, 33);
constexpr COLORREF kBorder = RGB(52, 68, 87);
constexpr COLORREF kText = RGB(238, 243, 249);
constexpr COLORREF kMuted = RGB(155, 174, 195);
constexpr COLORREF kGold = RGB(226, 187, 111);
constexpr COLORREF kGreen = RGB(123, 219, 171);
constexpr COLORREF kRed = RGB(247, 151, 151);
constexpr UINT kUpdate = WM_APP + 1;
constexpr UINT kFinished = WM_APP + 2;
constexpr UINT kAutoVerify = WM_APP + 3;
constexpr int kLogicalHeight = 660;
constexpr int kInitialLogicalWidth = 900;
constexpr int kBaseDpi = 96;
constexpr int kPointsPerInch = 72;
constexpr std::array kFontPoints{28, 13, 11, 9};
constexpr int kCaptionFont = 3;
constexpr COLORREF kGoldPressed = RGB(190, 151, 78);
constexpr COLORREF kGoldHover = RGB(243, 208, 146);
constexpr COLORREF kCardHover = RGB(41, 58, 78);
constexpr UINT kLeftAligned = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
constexpr UINT kRightAligned = DT_RIGHT | DT_VCENTER | DT_SINGLELINE;
constexpr UINT kCentered = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
constexpr int kEditMargin = 9;
constexpr int kCardRadius = 14;
constexpr int kBadgeRadius = 7;
constexpr int kFieldRadius = 8;
constexpr int kFocusInset = 4;
constexpr int kFieldPadding = 5;
constexpr int kMarqueeMilliseconds = 40;
constexpr int kContentInset = 44;
constexpr int kCardInset = 24;
constexpr int kFooterHeight = 110;
constexpr int kFooterGap = 20;
constexpr int kFocusScrollMargin = 8;
constexpr int kScrollLine = 28;
constexpr int kScrollPage = 160;
constexpr int kScrollWheel = 48;
constexpr int kMinimumWidth = 800;
constexpr int kMinimumHeight = 450;
constexpr int kWindowWidth = 940;
constexpr int kWindowHeight = 710;
constexpr int kWorkAreaMargin = 24;
constexpr INT_PTR kShellExecuteLastError = 32;
constexpr int kBadgeLeft = 44;
constexpr int kBadgeSize = 25;
constexpr int kSectionTitleLeft = 80;
constexpr int kSectionTitleInset = 240;
enum Control {
  kCue = 101,
  kSaves,
  kFfmpeg,
  kCueBrowse,
  kSaveBrowse,
  kFfmpegBrowse,
  kDetect,
  kDownload,
  kInstall,
  kPlay,
  kSetupLog,
  kVerify,
  kDetails,
  kDiscState,
  kFooterTitle,
  kFooterDetail,
  kMediaState,
  kProgress
};
enum class Job { kNone, kVerify, kPlay };

// How a control follows the window width: a fixed box, a box that ends at the
// content's right edge, or one that fills to `extent` before that edge.
enum class Span { kFixed, kFromRight, kStretch };
struct Slot {
  int id;
  Span span;
  int x;
  int y;
  int extent;
  int height;
  bool in_footer = false;
};
constexpr std::array<Slot, 18> kSlots{{
    {kCue, Span::kStretch, 45, 154, 169, 26},
    {kCueBrowse, Span::kFromRight, 0, 149, 112, 36},
    {kDiscState, Span::kStretch, 44, 197, 232, 24},
    {kVerify, Span::kFromRight, 0, 192, 168, 34},
    {kProgress, Span::kStretch, 44, 237, 44, 8},
    {kSaves, Span::kStretch, 45, 340, 169, 26},
    {kSaveBrowse, Span::kFromRight, 0, 335, 112, 36},
    {kFfmpeg, Span::kStretch, 45, 443, 169, 26},
    {kFfmpegBrowse, Span::kFromRight, 0, 438, 112, 36},
    {kMediaState, Span::kFromRight, 0, 409, 230, 22},
    {kDetect, Span::kFixed, 44, 486, 110, 32},
    {kInstall, Span::kFixed, 166, 486, 176, 32},
    {kDownload, Span::kFixed, 354, 486, 122, 32},
    {kDetails, Span::kFromRight, 0, 486, 120, 32},
    {kFooterTitle, Span::kStretch, 44, 16, 260, 25, true},
    {kFooterDetail, Span::kStretch, 44, 45, 260, 38, true},
    {kPlay, Span::kFromRight, 0, 21, 176, 48, true},
    {kSetupLog, Span::kFromRight, 0, 28, 120, 32},
}};

struct CardRows {
  int top;
  int bottom;
};
constexpr std::array<CardRows, 3> kCards{{{96, 274}, {286, 380}, {392, 536}}};

struct Section {
  const wchar_t* number;
  const wchar_t* title;
  int y;
};
constexpr std::array<Section, 3> kSections{{
    {L"01", L"Game disc", 112},
    {L"02", L"Save location", 302},
    {L"03", L"First-time media setup", 408},
}};

// A left edge is measured from the window's left when `left_inset` is zero and
// from its right otherwise.
struct Caption {
  const wchar_t* text;
  int left;
  int left_inset;
  int top;
  int right_inset;
  int bottom;
  int font;
  COLORREF color;
  UINT flags;
};
constexpr std::array<Caption, 5> kCaptions{{
    {L"Vagrant Story", 24, 0, 13, 190, 62, 0, kText, kLeftAligned},
    {L"NATIVE PORT  /  PLAYTEST EDITION", 26, 0, 66, 100, 84, kCaptionFont,
     kGold, kLeftAligned},
    {L"USA / SLUS-01040", 0, 260, 115, 44, 137, kCaptionFont, kMuted,
     kRightAligned},
    {L"Whole-file checksum verification. Renaming a file cannot bypass it.", 44,
     0, 250, 44, 266, kCaptionFont, kMuted, kLeftAligned},
    {L"Your existing saves are never moved or deleted.", 0, 450, 306, 44, 326,
     kCaptionFont, kMuted, kRightAligned},
}};

std::wstring Wide(const std::string& value) {
  return Platform::Utf8Path(value).wstring();
}

std::wstring Text(HWND control) {
  std::wstring result(
      static_cast<std::size_t>(GetWindowTextLengthW(control)) + 1, L'\0');
  const int count =
      GetWindowTextW(control, result.data(), static_cast<int>(result.size()));
  result.resize(count);
  return result;
}

void Error(HWND owner, const std::string& message) {
  MessageBoxW(owner, Wide(message).c_str(), L"Vagrant Story Native",
              MB_OK | MB_ICONERROR);
}

struct UiUpdate {
  SetupStage stage;
  std::string message;
  int percent = -1;
};
struct Completion {
  Job job;
  bool verified = false;
  std::string error;
};
struct Widget {
  int id;
  HWND window;
  RECT logical{};
  bool button = false;
};

struct App {
  HWND window = nullptr;
  fs::path installation;
  fs::path state;
  std::vector<Widget> widgets;
  std::array<HFONT, kFontPoints.size()> fonts{};
  HBRUSH card_brush = CreateSolidBrush(kCard);
  HBRUSH input_brush = CreateSolidBrush(kInput);
  std::thread worker;
  UINT dpi = kBaseDpi;
  int scroll = 0;
  int hover = 0;
  int focus = 0;
  int logical_width = kInitialLogicalWidth;
  int content_height = kLogicalHeight;
  bool built = false;
  bool verified = false;
  Job job = Job::kNone;
  COLORREF disc_color = kMuted;
  std::string last_error;

  ~App() {
    if (worker.joinable()) worker.join();
    for (auto font : fonts) {
      if (font) DeleteObject(font);
    }
    if (card_brush) DeleteObject(card_brush);
    if (input_brush) DeleteObject(input_brush);
  }
  bool Busy() const { return job != Job::kNone; }
  int Scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi), kBaseDpi);
  }
  HWND Find(int id) const {
    const auto found = std::ranges::find(widgets, id, &Widget::id);
    return found == widgets.end() ? nullptr : found->window;
  }
  void Set(int id, const std::wstring& text) const {
    if (const auto control = Find(id)) SetWindowTextW(control, text.c_str());
  }

  void Fonts() {
    std::array<HFONT, kFontPoints.size()> replacement{};
    for (std::size_t index = 0; index < replacement.size(); ++index) {
      replacement[index] = CreateFontW(
          -MulDiv(kFontPoints[index], static_cast<int>(dpi), kPointsPerInch), 0,
          0, 0, index < 2 ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
      if (!replacement[index]) {
        for (auto font : replacement) {
          if (font) DeleteObject(font);
        }
        throw std::runtime_error("Cannot create the launcher fonts.");
      }
    }
    fonts.swap(replacement);
    for (const auto& widget : widgets) {
      SendMessageW(
          widget.window, WM_SETFONT,
          reinterpret_cast<WPARAM>(fonts[widget.id == kFooterTitle ? 1 : 2]),
          TRUE);
    }
    for (auto font : replacement) {
      if (font) DeleteObject(font);
    }
  }

  static LRESULT CALLBACK ButtonProcedure(HWND control, UINT message,
                                          WPARAM wparam, LPARAM lparam,
                                          UINT_PTR id, DWORD_PTR data) {
    auto* app = reinterpret_cast<App*>(data);
    if (message == WM_MOUSEMOVE && app->hover != static_cast<int>(id)) {
      app->hover = static_cast<int>(id);
      TRACKMOUSEEVENT tracking{sizeof tracking, TME_LEAVE, control, 0};
      TrackMouseEvent(&tracking);
      InvalidateRect(control, nullptr, FALSE);
    } else if (message == WM_MOUSELEAVE) {
      app->hover = 0;
      InvalidateRect(control, nullptr, FALSE);
    } else if (message == WM_NCDESTROY) {
      RemoveWindowSubclass(control, ButtonProcedure, id);
    }
    return DefSubclassProc(control, message, wparam, lparam);
  }

  void Add(int id, const wchar_t* type, const std::wstring& text, DWORD style,
           bool button = false) {
    HWND control = CreateWindowExW(
        0, type, text.c_str(), WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1,
        window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (!control) throw std::runtime_error("Cannot create launcher controls.");
    widgets.push_back({id, control, {}, button});
    SendMessageW(control, WM_SETFONT,
                 reinterpret_cast<WPARAM>(fonts[id == kFooterTitle ? 1 : 2]),
                 TRUE);
    if (button &&
        !SetWindowSubclass(control, ButtonProcedure, static_cast<UINT_PTR>(id),
                           reinterpret_cast<DWORD_PTR>(this))) {
      throw std::runtime_error(
          "Cannot initialize launcher button interaction.");
    }
    if (std::wstring_view(type) == L"EDIT") {
      SendMessageW(control, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                   MAKELPARAM(Scale(kEditMargin), Scale(kEditMargin)));
    }
  }

  void Build(const Settings& settings) {
    if (!card_brush || !input_brush) {
      throw std::runtime_error("Cannot initialize launcher colors.");
    }
    dpi = GetDpiForWindow(window);
    Fonts();
    const auto edit = WS_TABSTOP | ES_AUTOHSCROLL;
    const auto button = WS_TABSTOP | BS_OWNERDRAW | BS_NOTIFY;
    Add(kCue, L"EDIT", settings.cue.wstring(), edit);
    Add(kCueBrowse, L"BUTTON", L"Choose CUE...", button, true);
    Add(kDiscState, L"STATIC", L"No disc selected", SS_LEFT);
    Add(kVerify, L"BUTTON", L"Verify disc", button, true);
    Add(kProgress, PROGRESS_CLASSW, L"", PBS_SMOOTH);
    Add(kSaves, L"EDIT", settings.saves.wstring(), edit);
    Add(kSaveBrowse, L"BUTTON", L"Choose folder...", button, true);
    Add(kFfmpeg, L"EDIT", settings.ffmpeg.wstring(), edit);
    Add(kFfmpegBrowse, L"BUTTON", L"Browse...", button, true);
    Add(kMediaState, L"STATIC",
        settings.ffmpeg.empty() ? L"FFmpeg not found" : L"FFmpeg path selected",
        SS_RIGHT);
    Add(kDetect, L"BUTTON", L"Detect", button, true);
    Add(kInstall, L"BUTTON", L"Install with winget", button, true);
    Add(kDownload, L"BUTTON", L"Download", button, true);
    Add(kDetails, L"BUTTON", L"Disc details", button, true);
    Add(kFooterTitle, L"STATIC", L"Verify your disc to begin", SS_LEFT);
    Add(kFooterDetail, L"STATIC",
        L"Only the supported USA dump can prepare assets or start the game.",
        SS_LEFT);
    Add(kPlay, L"BUTTON", L"Play", button, true);
    Add(kSetupLog, L"BUTTON", L"Setup log", button, true);
    SendMessageW(
        Find(kCue), EM_SETCUEBANNER, TRUE,
        reinterpret_cast<LPARAM>(L"Select the CUE that belongs to your BIN"));
    SendMessageW(Find(kFfmpeg), EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(
                     L"Click Detect, or choose your own ffmpeg.exe"));
    SendMessageW(Find(kSaves), EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(
                     L"Choose where your game saves will be stored"));
    SendMessageW(Find(kProgress), PBM_SETRANGE32, 0, kPercentComplete);
    if (FAILED(SetWindowTheme(Find(kProgress), L"", L""))) {
      throw std::runtime_error("Cannot initialize the progress display.");
    }
    SendMessageW(Find(kProgress), PBM_SETBKCOLOR, 0, kInput);
    SendMessageW(Find(kProgress), PBM_SETBARCOLOR, 0, kGold);
    built = true;
    Layout();
    Enable();
  }

  void Place(int id, int x, int y, int width, int height) {
    auto found = std::ranges::find(widgets, id, &Widget::id);
    if (found == widgets.end()) return;
    found->logical = {x, y, x + width, y + height};
    MoveWindow(found->window, Scale(x), Scale(y - scroll), Scale(width),
               Scale(height), TRUE);
  }

  void Layout() {
    if (!built) return;
    RECT client{};
    GetClientRect(window, &client);
    logical_width = MulDiv(client.right, kBaseDpi, static_cast<int>(dpi));
    const int height = MulDiv(client.bottom, kBaseDpi, static_cast<int>(dpi));
    content_height = std::max(kLogicalHeight, height);
    scroll = std::clamp(scroll, 0, std::max(0, content_height - height));
    SCROLLINFO info{sizeof info, SIF_RANGE | SIF_PAGE | SIF_POS, 0, 0, 0, 0, 0};
    info.nMin = 0;
    info.nMax = content_height - 1;
    info.nPage = static_cast<UINT>(height);
    info.nPos = scroll;
    SetScrollInfo(window, SB_VERT, &info, TRUE);
    const int right = logical_width - kContentInset;
    const int footer = content_height - kFooterHeight;
    for (const auto& slot : kSlots) {
      const int x =
          slot.span == Span::kFromRight ? right - slot.extent : slot.x;
      const int width =
          slot.span == Span::kStretch ? right - slot.extent : slot.extent;
      Place(slot.id, x, slot.y + (slot.in_footer ? footer : 0), width,
            slot.height);
    }
    InvalidateRect(window, nullptr, FALSE);
  }

  void Enable() {
    for (const auto& widget : widgets) {
      if (!widget.button && widget.id != kCue && widget.id != kSaves &&
          widget.id != kFfmpeg) {
        continue;
      }
      const bool utility = widget.id == kSetupLog || widget.id == kDetails;
      EnableWindow(widget.window, utility || !Busy());
    }
    EnableWindow(Find(kPlay),
                 !Busy() && verified && !Text(Find(kSaves)).empty());
    EnableWindow(Find(kVerify), !Busy() && !Text(Find(kCue)).empty());
    InvalidateRect(window, nullptr, FALSE);
  }

  RECT Pixels(RECT logical) const {
    return {Scale(logical.left), Scale(logical.top - scroll),
            Scale(logical.right), Scale(logical.bottom - scroll)};
  }

  static void Round(HDC dc, RECT rect, COLORREF fill, COLORREF border,
                    int radius) {
    const auto old_brush = SelectObject(dc, GetStockObject(DC_BRUSH));
    const auto old_pen = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCBrushColor(dc, fill);
    SetDCPenColor(dc, border);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
  }

  void DrawTextAt(HDC dc, const wchar_t* text, RECT logical, int font,
                  COLORREF color, UINT flags = kLeftAligned) {
    const auto previous = SelectObject(dc, fonts[font]);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    auto rect = Pixels(logical);
    DrawTextW(dc, text, -1, &rect, flags);
    SelectObject(dc, previous);
  }

  void Paint(HDC dc) {
    RECT client{};
    GetClientRect(window, &client);
    SetDCBrushColor(dc, kCanvas);
    FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    if (!built) return;
    const int right = logical_width - kCardInset;
    for (const auto& card : kCards) {
      Round(dc, Pixels({kCardInset, card.top, right, card.bottom}), kCard,
            kBorder, Scale(kCardRadius));
    }
    Round(dc,
          Pixels({kCardInset, content_height - kFooterHeight, right,
                  content_height - kFooterGap}),
          kCard, kBorder, Scale(kCardRadius));
    for (const auto& caption : kCaptions) {
      const int left = caption.left_inset != 0
                           ? logical_width - caption.left_inset
                           : caption.left;
      DrawTextAt(dc, caption.text,
                 {left, caption.top, logical_width - caption.right_inset,
                  caption.bottom},
                 caption.font, caption.color, caption.flags);
    }
    for (const auto& section : kSections) {
      const RECT badge{kBadgeLeft, section.y, kBadgeLeft + kBadgeSize,
                       section.y + kBadgeSize};
      Round(dc, Pixels(badge), kInput, kBorder, Scale(kBadgeRadius));
      DrawTextAt(dc, section.number, badge, kCaptionFont, kGold, kCentered);
      DrawTextAt(
          dc, section.title,
          {kSectionTitleLeft, section.y - 1, logical_width - kSectionTitleInset,
           section.y + kBadgeSize + 1},
          1, kText);
    }
    for (const int id : {kCue, kSaves, kFfmpeg}) {
      const auto found = std::ranges::find(widgets, id, &Widget::id);
      auto rect = found->logical;
      rect.left -= 1;
      rect.top -= kFieldPadding;
      rect.right += 1;
      rect.bottom += kFieldPadding;
      Round(dc, Pixels(rect), kInput, focus == id ? kGold : kBorder,
            Scale(kFieldRadius));
    }
  }

  void DrawButton(const DRAWITEMSTRUCT& item) {
    const int id = static_cast<int>(item.CtlID);
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    COLORREF fill = id == kPlay ? kGold : kCard;
    COLORREF text = id == kPlay ? kCanvas : kText;
    if (disabled) {
      fill = kInput;
      text = kMuted;
    } else if (pressed) {
      fill = id == kPlay ? kGoldPressed : kInput;
    } else if (hover == id) {
      fill = id == kPlay ? kGoldHover : kCardHover;
    }
    SetDCBrushColor(item.hDC, kCard);
    FillRect(item.hDC, &item.rcItem,
             static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    Round(item.hDC, item.rcItem, fill,
          id == kPlay && !disabled ? fill : kBorder, Scale(kFieldRadius));
    const auto font = SelectObject(item.hDC, fonts[id == kPlay ? 1 : 2]);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, text);
    auto rect = item.rcItem;
    const auto label = Text(item.hwndItem);
    DrawTextW(item.hDC, label.c_str(), -1, &rect, kCentered);
    if (item.itemState & ODS_FOCUS) {
      InflateRect(&rect, -Scale(kFocusInset), -Scale(kFocusInset));
      DrawFocusRect(item.hDC, &rect);
    }
    SelectObject(item.hDC, font);
  }

  Settings CurrentSettings() const {
    const auto path = [](std::wstring value) {
      return value.empty() ? fs::path{} : fs::absolute(fs::path(value));
    };
    return {path(Text(Find(kCue))), path(Text(Find(kSaves))),
            path(Text(Find(kFfmpeg)))};
  }

  void Bar(int percent) {
    if (percent < 0) {
      SetWindowLongPtrW(
          Find(kProgress), GWL_STYLE,
          GetWindowLongPtrW(Find(kProgress), GWL_STYLE) | PBS_MARQUEE);
      SendMessageW(Find(kProgress), PBM_SETMARQUEE, TRUE, kMarqueeMilliseconds);
    } else {
      SendMessageW(Find(kProgress), PBM_SETMARQUEE, FALSE, 0);
      SetWindowLongPtrW(
          Find(kProgress), GWL_STYLE,
          GetWindowLongPtrW(Find(kProgress), GWL_STYLE) & ~PBS_MARQUEE);
      SendMessageW(Find(kProgress), PBM_SETPOS,
                   std::clamp(percent, 0, kPercentComplete), 0);
    }
  }

  void VerifyJob(const Settings& settings, Completion& completion) const {
    const auto result = VerifyDisc(settings.cue, [this](int percent) {
      Post(kUpdate,
           UiUpdate{SetupStage::kDisc, "Verifying BIN checksum...", percent});
    });
    completion.verified = result.has_value();
    if (!result) completion.error = result.error();
  }

  void PlayJob(const Settings& settings, Completion& completion) const {
    const auto data = Prepare(
        settings, installation, state,
        [this, &completion](const ProgressUpdate& update) {
          if (update.stage != SetupStage::kDisc) {
            completion.verified = true;
          }
          Post(kUpdate, UiUpdate{update.stage, update.message, update.percent});
        });
    if (!data) {
      completion.error = data.error();
      return;
    }
    Post(kUpdate, UiUpdate{SetupStage::kReady,
                           "Close the game window to return to the launcher.",
                           kPercentComplete});
    const auto result =
        Platform::RunProcess(GameArguments(installation, *data, settings));
    if (!result || *result != 0) {
      completion.error = "The game did not exit normally: " +
                         (result ? std::to_string(*result) : result.error());
    }
  }

  void Work(const Settings& settings, Job requested) const {
    Completion completion{requested, false, {}};
    try {
      if (requested == Job::kVerify) {
        VerifyJob(settings, completion);
      } else {
        PlayJob(settings, completion);
      }
    } catch (const std::exception& error) {
      completion.error = error.what();
    }
    Post(kFinished, std::move(completion));
  }

  template <class T>
  void Post(UINT kind, T value) const {
    auto message = std::make_unique<T>(std::move(value));
    if (!PostMessageW(window, kind, 0,
                      reinterpret_cast<LPARAM>(message.get()))) {
      Error(nullptr, "Cannot deliver launcher progress.");
      return;
    }
    message.release();
  }

  void Begin(Job requested) {
    if (Busy()) return;
    const auto settings = CurrentSettings();
    if (settings.cue.empty()) {
      Error(window, "Choose the CUE file for your game disc first.");
      return;
    }
    if (requested == Job::kPlay && !verified) {
      Error(window, "Verify the selected disc before pressing Play.");
      return;
    }
    if (requested == Job::kPlay) {
      if (auto result = SaveSettings(state, settings); !result) {
        Error(window, result.error());
        return;
      }
    }
    worker = std::thread(&App::Work, this, settings, requested);
    job = requested;
    verified = false;
    last_error.clear();
    disc_color = kGold;
    Set(kDiscState, L"Checking disc identity...");
    Set(kFooterTitle, L"Verifying your disc");
    Set(kFooterDetail,
        L"The whole BIN is checked before any assets or game code are used.");
    Bar(0);
    Enable();
  }

  void Update(const UiUpdate& update) {
    Bar(update.percent);
    if (update.stage == SetupStage::kDisc) {
      Set(kDiscState, L"Verifying BIN: " +
                          std::to_wstring(std::max(update.percent, 0)) + L"%");
      Set(kFooterDetail,
          L"Comparing the complete image with the supported USA dump.");
    } else {
      verified = true;
      disc_color = kGreen;
      Set(kDiscState, L"USA disc verified - SHA-256 matches");
      Set(kFooterTitle, update.stage == SetupStage::kReady
                            ? L"Game session"
                            : L"Preparing your game");
      Set(kFooterDetail, Wide(update.message));
    }
    InvalidateRect(window, nullptr, FALSE);
  }

  void Finish(const Completion& completion) {
    if (worker.joinable()) worker.join();
    job = Job::kNone;
    verified = completion.verified;
    last_error = completion.error;
    Bar(verified ? kPercentComplete : 0);
    if (!last_error.empty()) {
      disc_color = verified ? kGreen : kRed;
      Set(kDiscState, verified ? L"USA disc verified - setup needs attention"
                               : L"Disc verification failed");
      Set(kFooterTitle,
          verified ? L"Setup needs attention" : L"Unsupported disc selection");
      Set(kFooterDetail, Wide(last_error.substr(0, last_error.find('\n'))));
      Set(kDetails, L"View error");
    } else {
      disc_color = kGreen;
      Set(kDiscState, L"USA disc verified - SHA-256 matches");
      Set(kFooterTitle, completion.job == Job::kVerify
                            ? L"Ready to play"
                            : L"Ready for another session");
      Set(kFooterDetail,
          L"Your disc is supported. First play prepares assets automatically.");
      Set(kDetails, L"Disc details");
    }
    Enable();
  }

  void InvalidateDisc() {
    if (!built || Busy()) return;
    verified = false;
    last_error.clear();
    disc_color = kMuted;
    Set(kDiscState, Text(Find(kCue)).empty()
                        ? L"No disc selected"
                        : L"Disc selection changed - verify to continue");
    Set(kFooterTitle, L"Verify your disc to begin");
    Set(kFooterDetail,
        L"Only the exact supported USA dump can start the game.");
    Set(kDetails, L"Disc details");
    Bar(0);
    Enable();
  }

  void Browse(int id, bool folder, const wchar_t* pattern) {
    ComPtr<IFileOpenDialog> dialog;
    HRESULT result =
        CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&dialog));
    if (FAILED(result)) {
      throw std::runtime_error("Cannot open the Windows file picker.");
    }
    DWORD options = 0;
    result = dialog->GetOptions(&options);
    if (SUCCEEDED(result)) {
      result =
          dialog->SetOptions(options | FOS_FORCEFILESYSTEM |
                             (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    }
    const COMDLG_FILTERSPEC filter{pattern, pattern};
    if (SUCCEEDED(result) && !folder) result = dialog->SetFileTypes(1, &filter);
    if (FAILED(result)) {
      throw std::runtime_error("Cannot configure the file picker.");
    }
    result = dialog->Show(window);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return;
    if (FAILED(result)) throw std::runtime_error("The file picker failed.");
    ComPtr<IShellItem> selection;
    PWSTR path = nullptr;
    result = dialog->GetResult(&selection);
    if (SUCCEEDED(result)) {
      result = selection->GetDisplayName(SIGDN_FILESYSPATH, &path);
    }
    if (FAILED(result)) {
      throw std::runtime_error("Cannot read the selected path.");
    }
    Set(id, path);
    CoTaskMemFree(path);
    if (id == kCue) Begin(Job::kVerify);
  }

  void Open(const fs::path& path, const std::wstring& arguments = L"") {
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(window, L"open", path.c_str(),
                      arguments.empty() ? nullptr : arguments.c_str(), nullptr,
                      SW_SHOWNORMAL));
    if (result <= kShellExecuteLastError) {
      throw std::runtime_error("Windows could not open the requested item.");
    }
  }

  void Details() const {
    const std::string supported =
        "Supported disc: Vagrant Story USA (SLUS-01040)\n"
        "Single BINARY file / TRACK 01 MODE2/2352 / INDEX 01 00:00:00\n"
        "BIN size: " +
        std::to_string(kSupportedDiscBytes) +
        " bytes\nSHA-1: " + std::string(kSupportedDiscSha1) +
        "\nSHA-256: " + std::string(kSupportedDiscSha256) +
        "\n\nA different region, modified image or incomplete dump is "
        "rejected. "
        "The CUE filename may differ; its referenced BIN must match exactly.";
    MessageBoxW(
        window,
        Wide(last_error.empty() ? supported : last_error + "\n\n" + supported)
            .c_str(),
        L"Disc verification details",
        MB_OK | (last_error.empty() ? MB_ICONINFORMATION : MB_ICONERROR));
  }

  void Command(int id, int notification) {
    if (!built) return;
    if (notification == EN_CHANGE && id == kCue) {
      InvalidateDisc();
      return;
    }
    if (notification == EN_CHANGE && id == kSaves) {
      Enable();
      return;
    }
    if (notification == EN_CHANGE && id == kFfmpeg) {
      Set(kMediaState, Text(Find(kFfmpeg)).empty() ? L"FFmpeg not found"
                                                   : L"FFmpeg path selected");
      return;
    }
    if (notification == EN_SETFOCUS || notification == BN_SETFOCUS) {
      focus = id;
      const auto found = std::ranges::find(widgets, id, &Widget::id);
      if (found != widgets.end()) {
        RECT client{};
        GetClientRect(window, &client);
        const int visible =
            MulDiv(client.bottom, kBaseDpi, static_cast<int>(dpi));
        if (found->logical.top < scroll) scroll = found->logical.top;
        if (found->logical.bottom > scroll + visible) {
          scroll = found->logical.bottom - visible + kFocusScrollMargin;
        }
        Layout();
      }
      return;
    }
    if (notification != BN_CLICKED) return;
    switch (id) {
      case kCueBrowse:
        Browse(kCue, false, L"*.cue");
        break;
      case kSaveBrowse:
        Browse(kSaves, true, L"");
        break;
      case kFfmpegBrowse:
        Browse(kFfmpeg, false, L"ffmpeg.exe");
        break;
      case kVerify:
        Begin(Job::kVerify);
        break;
      case kPlay:
        Begin(Job::kPlay);
        break;
      case kDetails:
        Details();
        break;
      case kDetect: {
        const auto path = FindFfmpeg();
        Set(kFfmpeg, path.wstring());
        Set(kMediaState,
            path.empty() ? L"FFmpeg not found" : L"FFmpeg detected");
        if (path.empty()) {
          Error(window,
                "Choose ffmpeg.exe, use Download, or Install with winget.");
        }
        break;
      }
      case kDownload:
        Open(L"https://ffmpeg.org/download.html#build-windows");
        break;
      case kInstall: {
        const auto script = installation / "Check-Dependencies.ps1";
        if (!fs::is_regular_file(script)) {
          throw std::runtime_error(
              "Check-Dependencies.ps1 is missing. Extract the entire ZIP.");
        }
        Open(L"powershell.exe", L"-NoProfile -ExecutionPolicy Bypass -File \"" +
                                    script.wstring() +
                                    L"\" -InstallFfmpeg -Pause");
        Set(kFooterDetail, L"Finish installation, then click Detect and Play.");
        break;
      }
      case kSetupLog:
        if (fs::exists(state / "setup.log")) {
          Open(state / "setup.log");
        } else {
          Error(window,
                "No setup log yet. Setup output appears here after preparation "
                "starts.");
        }
        break;
      default:
        break;
    }
  }

  LRESULT ColorControl(HDC dc, HWND control) const {
    const int id = GetDlgCtrlID(control);
    const bool edit = id == kCue || id == kSaves || id == kFfmpeg;
    SetTextColor(dc, control == Find(kDiscState) ? disc_color : kText);
    SetBkColor(dc, edit ? kInput : kCard);
    return reinterpret_cast<LRESULT>(edit ? input_brush : card_brush);
  }

  void DpiChanged(UINT changed, const RECT& rect) {
    dpi = changed;
    Fonts();
    SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
    Layout();
  }

  void VerticalScroll(int action) {
    SCROLLINFO info{sizeof info, SIF_TRACKPOS, 0, 0, 0, 0, 0};
    GetScrollInfo(window, SB_VERT, &info);
    if (action == SB_THUMBTRACK) {
      scroll = info.nTrackPos;
    } else if (action == SB_LINEUP) {
      scroll -= kScrollLine;
    } else if (action == SB_LINEDOWN) {
      scroll += kScrollLine;
    } else if (action == SB_PAGEUP) {
      scroll -= kScrollPage;
    } else if (action == SB_PAGEDOWN) {
      scroll += kScrollPage;
    }
    Layout();
  }
};

// Returns the result of a message the launcher handled itself.
std::optional<LRESULT> Dispatch(App& app, HWND window, UINT message,
                                WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_COMMAND:
      app.Command(LOWORD(wparam), HIWORD(wparam));
      return 0;
    case WM_DRAWITEM:
      if (app.built) {
        app.DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
      }
      return TRUE;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      const auto dc = BeginPaint(window, &paint);
      app.Paint(dc);
      EndPaint(window, &paint);
      return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
      return app.ColorControl(reinterpret_cast<HDC>(wparam),
                              reinterpret_cast<HWND>(lparam));
    case WM_SIZE:
      app.Layout();
      return 0;
    case WM_DPICHANGED:
      app.DpiChanged(HIWORD(wparam), *reinterpret_cast<RECT*>(lparam));
      return 0;
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
      info->ptMinTrackSize = {app.Scale(kMinimumWidth),
                              app.Scale(kMinimumHeight)};
      return 0;
    }
    case WM_VSCROLL:
      app.VerticalScroll(LOWORD(wparam));
      return 0;
    case WM_MOUSEWHEEL:
      app.scroll -= GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA * kScrollWheel;
      app.Layout();
      return 0;
    case kAutoVerify:
      if (!Text(app.Find(kCue)).empty()) app.Begin(Job::kVerify);
      return 0;
    case kUpdate: {
      const std::unique_ptr<UiUpdate> update(
          reinterpret_cast<UiUpdate*>(lparam));
      app.Update(*update);
      return 0;
    }
    case kFinished: {
      const std::unique_ptr<Completion> completion(
          reinterpret_cast<Completion*>(lparam));
      app.Finish(*completion);
      return 0;
    }
    case WM_CLOSE:
      if (app.Busy()) {
        Error(window,
              "A disc check, setup or game session is active. Wait for "
              "setup or close the game first.");
        return 0;
      }
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam,
                                 LPARAM lparam) {
  auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    app = static_cast<App*>(
        reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    app->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  }
  if (app) {
    try {
      if (const auto handled =
              Dispatch(*app, window, message, wparam, lparam)) {
        return *handled;
      }
    } catch (const std::exception& error) {
      Error(window, error.what());
      return 0;
    }
  }
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}
}  // namespace

int ShowLauncher(const Settings& settings, const fs::path& installation,
                 const fs::path& state, int show) {
  INITCOMMONCONTROLSEX common{sizeof common,
                              ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS};
  if (!InitCommonControlsEx(&common)) {
    throw std::runtime_error("Cannot initialize Windows controls.");
  }
  App app;
  app.installation = installation;
  app.state = state;
  app.dpi = GetDpiForSystem();
  WNDCLASSW type{};
  type.lpfnWndProc = WindowProcedure;
  type.hInstance = GetModuleHandleW(nullptr);
  type.hCursor = LoadCursor(nullptr, IDC_ARROW);
  type.lpszClassName = L"VSNativePortLauncher";
  if (!RegisterClassW(&type)) {
    throw std::runtime_error("Cannot register launcher window.");
  }
  RECT work{};
  if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
    throw std::runtime_error("Cannot read screen work area.");
  }
  const LONG width =
      std::min<LONG>(app.Scale(kWindowWidth),
                     work.right - work.left - app.Scale(kWorkAreaMargin));
  const LONG height =
      std::min<LONG>(app.Scale(kWindowHeight),
                     work.bottom - work.top - app.Scale(kWorkAreaMargin));
  const HWND window =
      CreateWindowExW(0, type.lpszClassName, L"Vagrant Story Native - Launcher",
                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
                      work.left + (work.right - work.left - width) / 2,
                      work.top + (work.bottom - work.top - height) / 2, width,
                      height, nullptr, nullptr, type.hInstance, &app);
  if (!window) throw std::runtime_error("Cannot create launcher window.");
  app.Build(settings);
  ShowWindow(window, show);
  PostMessageW(window, kAutoVerify, 0, 0);
  MSG message{};
  int result = 0;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  if (result < 0) throw std::runtime_error("The launcher event loop failed.");
  return 0;
}
}  // namespace VS::Launcher
