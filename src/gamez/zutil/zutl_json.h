#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Ours (docs/modding.md, mech3ax's JSON): a JSON document as values, for the
// formats unzbd writes as objects (motion; later mechlib, anim). The reader's
// entries, lists only, have their own (zrdr::parse_json). Errors name the
// line and the value's place as a path: "line 7 (at $.parts[0].frames[3]): ...".
namespace zutl_json {

struct Value {
  enum Kind : uint8_t { Null, Bool, Number, String, Array, Object } kind = Null;
  bool b = false;
  std::string text;                 // a number as written; a string's UTF-8
  int line = 0;                     // where it starts
  std::vector<Value> items;         // an array's
  std::vector<std::pair<std::string, Value>> members;  // an object's, in order

  const Value* get(const char* key) const;  // an object's member, or null
};

// Null with `why` set when the text is not one JSON value (depth past 64 too).
bool parse(const std::string& text, Value* out, std::string* why);

// Reading values against what a format expects: each failure sets `why` once
// with the value's line and path ("line N (at $.a[2].b): what").
struct Reader {
  std::string why;
  bool fail(const Value& v, const std::string& path, const std::string& what);
  // The member, or a failure "no KEY" when it is missing.
  const Value* member(const Value& obj, const std::string& path, const char* key);
  bool object(const Value& v, const std::string& path);
  bool array(const Value& v, const std::string& path);
  // A float read once from the text (strtof, correctly rounded): mech3ax writes
  // each f32 as its shortest text, so the bits come back. Range-checked.
  bool f32(const Value& v, const std::string& path, float* out);
  // A whole number within the range given.
  bool u32(const Value& v, const std::string& path, uint32_t* out, uint32_t most = 0xffffffffu);
  // A string the game's text (Windows-1252) holds, at most `most` bytes.
  bool game_string(const Value& v, const std::string& path, std::string* out, size_t most = 0xffff);
};

// A large document walked a value at a time, for the files too big for one
// tree (unzbd's gamez models.json is 41 MB): the reader steps into objects
// and lists, and reads each item it wants as its own small tree. The same
// line-and-path errors; each item's values carry their line in the document.
class Pull {
 public:
  explicit Pull(const std::string& text);
  ~Pull();
  Pull(const Pull&) = delete;
  Pull& operator=(const Pull&) = delete;
  bool begin_object();                   // '{', or a failure
  // The next member's name, true; false at the object's end (taken), or on a failure (why set).
  bool member(std::string* key);
  bool begin_list();                     // '['
  // True for another item (its path "[i]" set); false at the list's end (taken) or a failure.
  bool item();
  bool value(Value* out);                // the next value whole, as a tree
  bool skip();                           // the next value, dropped
  bool end();                            // nothing after the document's value
  std::string path() const;              // "$", "$.models[12]"...
  bool fail(const std::string& what);    // at the current line and path
  const std::string& why() const;

 private:
  struct State;
  State* s_;
};

// The shortest text that reads back as this float (with a '.' or exponent).
std::string f32_text(float f);

}  // namespace zutl_json
