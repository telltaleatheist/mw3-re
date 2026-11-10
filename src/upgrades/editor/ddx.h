#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// FIX (A), docs/editor.md (The vehicle editor's pages): MFC 4.2's dialog data
// exchange (DDX_*, DDV_* in mfc42.dll) on a page's controls kept as data,
// so a ported DoDataExchange runs line for line. A control is what Win32
// keeps: an edit's text, a button's check, a combo's items and selection.
// The ImGui forms bind to these; nothing here knows ImGui.
namespace mech3 {

// A combo box's list (CB_* on a CBS_DROPDOWNLIST).
struct DlgCombo {
  struct Item {
    std::string text;
    int32_t data = 0;
  };
  std::vector<Item> items;
  int32_t cur = -1;     // CB_GETCURSEL; -1 none
  bool sorted = false;  // CBS_SORT

  void reset();                         // CB_RESETCONTENT
  int32_t add(const std::string& s);    // CB_ADDSTRING: its index (sorted: in order, no case)
  void set_data(int32_t i, int32_t d);  // CB_SETITEMDATA
  int32_t data(int32_t i) const;        // CB_GETITEMDATA: -1 (CB_ERR) out of range
  void set_cur(int32_t i);              // CB_SETCURSEL: out of range clears it
  // CB_SELECTSTRING from the start: the first item the text begins, no case;
  // -1 and the selection left as it was when none does.
  int32_t select_string(const std::string& s);
  std::vector<std::string> texts() const;
};

struct DlgControl {
  std::string text;     // an edit's (WM_GETTEXT / WM_SETTEXT)
  int32_t check = 0;    // a button's (BM_GETCHECK); a radio group's first: the button checked, -1 none
  int32_t max_check = 1;  // 1 a check box, 2 a three-state one (BS_AUTO3STATE)
  int32_t radios = 0;   // a radio group's first: its buttons (ids in order)
  int32_t limit = 0;    // EM_LIMITTEXT (DDV_MaxChars), 0 none
  DlgCombo combo;
};

// A dialog's controls by id.
struct Dialog {
  std::map<int32_t, DlgControl> controls;
  DlgControl& at(int32_t id) { return controls[id]; }
  // BM_SETCHECK: above what the button holds is its top (checked).
  void set_check(int32_t id, int32_t v);
};

// One UpdateData pass (CDataExchange). A failed field throws DdxFail, as
// pDX->Fail() does, after its message box; update_data catches it.
struct DataExchange {
  Dialog* dlg = nullptr;
  bool save = false;            // m_bSaveAndValidate
  int32_t last = 0;             // the last control exchanged (for DDV_*)
  std::vector<int32_t>* trace = nullptr;  // tests: each control exchanged, in order
};
struct DdxFail {};

// MFC's messages for a failed field (AFX_IDP_PARSE_*, the DDV prompts).
struct DdxMessage {
  int32_t control = 0;   // focused after the box, as Fail() does
  std::string text;
};
// The last pass's failure, cleared by each update_data; shown by the caller.
DdxMessage& ddx_message();

// CWnd::UpdateData: false when a field failed (the fields before it taken).
template <class F>
bool update_data(Dialog& d, bool save, F&& exchange, std::vector<int32_t>* trace = nullptr) {
  ddx_message() = {};
  DataExchange dx{&d, save, 0, trace};
  try {
    exchange(dx);
  } catch (const DdxFail&) {
    return false;
  }
  return true;
}

void ddx_text(DataExchange& dx, int32_t id, int32_t& v);   // "%d"
void ddx_text(DataExchange& dx, int32_t id, uint32_t& v);  // "%u"
void ddx_text(DataExchange& dx, int32_t id, float& v);     // "%.6g" (FLT_DIG), strtod back
void ddx_text(DataExchange& dx, int32_t id, std::string& v);
void ddx_check(DataExchange& dx, int32_t id, int32_t& v);
void ddx_radio(DataExchange& dx, int32_t id, int32_t& v);  // the group starting at id
void ddv_max_chars(DataExchange& dx, const std::string& v, int32_t n);
void ddv_min_max_int(DataExchange& dx, int32_t v, int32_t lo, int32_t hi);
void ddv_min_max_float(DataExchange& dx, float v, float lo, float hi);  // the bounds shown as "%.6g"

// The conversions alone (the tests): the text a float shows as (MSVC's
// printf: three exponent digits), and whether a text reads back as each.
std::string ddx_float_text(float v);
bool ddx_parse_float(const std::string& s, double& d);
bool ddx_parse_int(const std::string& s, bool is_unsigned, int32_t& v);

}  // namespace mech3
