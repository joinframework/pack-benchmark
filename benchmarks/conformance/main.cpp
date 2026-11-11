#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "adapters/join.hpp"
#include "adapters/jsoncons.hpp"
#include "adapters/mpack.hpp"
#include "adapters/msgpack_cxx.hpp"
#include "adapters/nlohmann.hpp"
#include "adapters/taocpp.hpp"
#include "common/utils.hpp"

namespace fs = std::filesystem;

enum Category
{
    Core,
    BinaryExt,
    NonStringKeys,
    Malformed
};

static const char* const category_keys[] = {"core", "binary_ext", "non_string_keys"};

struct Score
{
    int pass = 0;
    int fail = 0;
};

struct Result
{
    Score accept[Malformed];
    Score roundtrip[Malformed];
    Score reject;
};

struct Case
{
    Category category;
    std::string name;
    std::string bytes;
};

static std::string hex (std::string_view text)
{
    std::string bytes;
    int high = -1;
    for (char c : text)
    {
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            continue;
        if (high < 0)
            high = digit;
        else
        {
            bytes.push_back (static_cast<char> (high * 16 + digit));
            high = -1;
        }
    }
    return bytes;
}

static std::vector<Case> valid_cases ()
{
    return {
        {Core, "nil", hex ("c0")},
        {Core, "false", hex ("c2")},
        {Core, "true", hex ("c3")},
        {Core, "positive fixint 0", hex ("00")},
        {Core, "positive fixint max", hex ("7f")},
        {Core, "negative fixint min", hex ("e0")},
        {Core, "negative fixint -1", hex ("ff")},
        {Core, "uint8 max", hex ("cc ff")},
        {Core, "uint16 max", hex ("cd ff ff")},
        {Core, "uint32 max", hex ("ce ff ff ff ff")},
        {Core, "uint64 max", hex ("cf ff ff ff ff ff ff ff ff")},
        {Core, "uint8 non-minimal", hex ("cc 01")},
        {Core, "uint64 non-minimal", hex ("cf 00 00 00 00 00 00 00 01")},
        {Core, "int8 min", hex ("d0 80")},
        {Core, "int8 max", hex ("d0 7f")},
        {Core, "int16 min", hex ("d1 80 00")},
        {Core, "int16 max", hex ("d1 7f ff")},
        {Core, "int32 min", hex ("d2 80 00 00 00")},
        {Core, "int32 max", hex ("d2 7f ff ff ff")},
        {Core, "int64 min", hex ("d3 80 00 00 00 00 00 00 00")},
        {Core, "int64 max", hex ("d3 7f ff ff ff ff ff ff ff")},
        {Core, "float32 1.5", hex ("ca 3f c0 00 00")},
        {Core, "float32 0.1", hex ("ca 3d cc cc cd")},
        {Core, "float32 max", hex ("ca 7f 7f ff ff")},
        {Core, "float32 min subnormal", hex ("ca 00 00 00 01")},
        {Core, "float32 -0.0", hex ("ca 80 00 00 00")},
        {Core, "float32 inf", hex ("ca 7f 80 00 00")},
        {Core, "float32 nan", hex ("ca 7f c0 00 00")},
        {Core, "float64 1.0", hex ("cb 3f f0 00 00 00 00 00 00")},
        {Core, "float64 0.1", hex ("cb 3f b9 99 99 99 99 99 9a")},
        {Core, "float64 -0.0", hex ("cb 80 00 00 00 00 00 00 00")},
        {Core, "float64 max", hex ("cb 7f ef ff ff ff ff ff ff")},
        {Core, "float64 min subnormal", hex ("cb 00 00 00 00 00 00 00 01")},
        {Core, "float64 inf", hex ("cb 7f f0 00 00 00 00 00 00")},
        {Core, "float64 -inf", hex ("cb ff f0 00 00 00 00 00 00")},
        {Core, "float64 nan", hex ("cb 7f f8 00 00 00 00 00 00")},
        {Core, "fixstr empty", hex ("a0")},
        {Core, "fixstr max", hex ("bf") + std::string (31, 'a')},
        {Core, "fixstr nul", hex ("a3 61 00 62")},
        {Core, "fixstr utf-8", hex ("a4 f0 9d 84 9e")},
        {Core, "str8 empty", hex ("d9 00")},
        {Core, "str8", hex ("d9 20") + std::string (32, 'a')},
        {Core, "str16", hex ("da 01 00") + std::string (256, 'a')},
        {Core, "str32", hex ("db 00 01 00 00") + std::string (65536, 'a')},
        {BinaryExt, "bin8 empty", hex ("c4 00")},
        {BinaryExt, "bin8", hex ("c4 03 01 02 03")},
        {BinaryExt, "bin16", hex ("c5 01 00") + std::string (256, '\xfe')},
        {BinaryExt, "bin32", hex ("c6 00 01 00 00") + std::string (65536, '\xfe')},
        {BinaryExt, "fixext1", hex ("d4 01 aa")},
        {BinaryExt, "fixext2", hex ("d5 02 aa bb")},
        {BinaryExt, "fixext4", hex ("d6 03 01 02 03 04")},
        {BinaryExt, "fixext8", hex ("d7 04 01 02 03 04 05 06 07 08")},
        {BinaryExt, "fixext16", hex ("d8 05") + std::string (16, '\x01')},
        {BinaryExt, "ext8 empty", hex ("c7 00 06")},
        {BinaryExt, "ext8", hex ("c7 03 07 01 02 03")},
        {BinaryExt, "ext16", hex ("c8 01 00 08") + std::string (256, '\x01')},
        {BinaryExt, "ext32", hex ("c9 00 01 00 00 09") + std::string (65536, '\x01')},
        {BinaryExt, "timestamp32", hex ("d6 ff 00 00 00 01")},
        {BinaryExt, "timestamp64", hex ("d7 ff 00 00 00 04 00 00 00 01")},
        {BinaryExt, "timestamp96", hex ("c7 0c ff 00 00 00 01 ff ff ff ff ff ff ff ff")},
        {Core, "empty array", hex ("90")},
        {Core, "empty map", hex ("80")},
        {Core, "nested empty arrays", hex ("91 91 90")},
        {Core, "array16", hex ("dc 00 10") + std::string (16, '\x01')},
        {Core, "array16 non-minimal", hex ("dc 00 01 c0")},
        {Core, "array32", hex ("dd 00 00 00 02 01 02")},
        {Core, "map16", hex ("de 00 01 a1 61 01")},
        {Core, "map32", hex ("df 00 00 00 01 a1 61 01")},
        {Core, "mixed array", hex ("96 c0 c3 01 e0 a1 61 cb 3f f8 00 00 00 00 00 00")},
        {Core, "nested map", hex ("81 a1 61 81 a1 62 92 01 80")},
        {NonStringKeys, "map int key", hex ("81 01 a1 61")},
        {NonStringKeys, "map negative int key", hex ("81 ff a1 61")},
        {NonStringKeys, "map uint64 key", hex ("81 cf ff ff ff ff ff ff ff ff c0")},
        {NonStringKeys, "map nil key", hex ("81 c0 01")},
        {NonStringKeys, "map bool keys", hex ("82 c2 00 c3 01")},
        {NonStringKeys, "map float key", hex ("81 cb 3f f8 00 00 00 00 00 00 01")},
        {NonStringKeys, "map bin key", hex ("81 c4 01 00 01")},
        {NonStringKeys, "map array key", hex ("81 92 01 02 03")},
        {NonStringKeys, "map map key", hex ("81 81 a1 61 01 02")},
        {Core, "deep nesting", std::string (100, '\x91') + hex ("c0")},
    };
}

static std::vector<Case> invalid_cases ()
{
    return {
        {Malformed, "empty input", ""},
        {Malformed, "never used 0xc1", hex ("c1")},
        {Malformed, "never used 0xc1 in array", hex ("91 c1")},
        {Malformed, "never used 0xc1 as key", hex ("81 c1 01")},
        {Malformed, "truncated uint8", hex ("cc")},
        {Malformed, "truncated uint16", hex ("cd 01")},
        {Malformed, "truncated uint32", hex ("ce 01 02 03")},
        {Malformed, "truncated uint64", hex ("cf 01 02 03 04 05 06 07")},
        {Malformed, "truncated int8", hex ("d0")},
        {Malformed, "truncated int16", hex ("d1 01")},
        {Malformed, "truncated int32", hex ("d2 01 02")},
        {Malformed, "truncated int64", hex ("d3 01")},
        {Malformed, "truncated float32", hex ("ca 00 00 00")},
        {Malformed, "truncated float64", hex ("cb 00 00 00 00 00 00 00")},
        {Malformed, "truncated fixstr", hex ("a3 61 62")},
        {Malformed, "truncated str8 header", hex ("d9")},
        {Malformed, "truncated str8", hex ("d9 05 61 62")},
        {Malformed, "truncated str16", hex ("da 00 05 61")},
        {Malformed, "truncated str32", hex ("db 00 00 00 05 61")},
        {Malformed, "huge str32", hex ("db ff ff ff ff 61")},
        {Malformed, "truncated bin8", hex ("c4 03 01")},
        {Malformed, "truncated bin16 header", hex ("c5 00")},
        {Malformed, "huge bin32", hex ("c6 ff ff ff ff 01")},
        {Malformed, "truncated fixext1", hex ("d4 01")},
        {Malformed, "truncated fixext16", hex ("d8 05 01 02 03 04 05 06 07 08")},
        {Malformed, "truncated ext8 type", hex ("c7 01")},
        {Malformed, "truncated ext8", hex ("c7 05 01 aa")},
        {Malformed, "truncated array", hex ("92 01")},
        {Malformed, "truncated array16", hex ("dc 00 03 01")},
        {Malformed, "huge array32", hex ("dd ff ff ff ff 01")},
        {Malformed, "truncated map key", hex ("82 a1 61 01")},
        {Malformed, "truncated map value", hex ("81 a1 61")},
        {Malformed, "huge map32", hex ("df ff ff ff ff a1 61 01")},
        {Malformed, "truncated nested array", hex ("91 91 92 01")},
        {Malformed, "trailing byte", hex ("c3 00")},
    };
}

static bool doubles_bitwise_eq (double a, double b)
{
    uint64_t ua, ub;
    std::memcpy (&ua, &a, 8);
    std::memcpy (&ub, &b, 8);
    return ua == ub;
}

struct Node
{
    enum Kind
    {
        Nil,
        Bool,
        Int,
        Uint,
        Float,
        Str,
        Bin,
        Ext,
        Array,
        Map
    };

    Kind kind = Nil;
    bool boolean = false;
    int64_t integer = 0;
    uint64_t uinteger = 0;
    double real = 0;
    int8_t ext_type = 0;
    std::string bytes;
    std::vector<Node> items;
    std::vector<std::pair<Node, Node>> members;
};

class Decoder
{
public:
    explicit Decoder (std::string_view in)
    : _in (in)
    {
    }

    std::optional<Node> decode ()
    {
        Node node;
        if (!value (node) || _pos != _in.size ())
            return std::nullopt;
        return node;
    }

private:
    bool take (uint64_t size, std::string_view& out)
    {
        if (size > _in.size () - _pos)
            return false;
        out = _in.substr (_pos, size);
        _pos += size;
        return true;
    }

    bool number (size_t width, uint64_t& out)
    {
        std::string_view raw;
        if (!take (width, raw))
            return false;
        out = 0;
        for (char c : raw)
            out = (out << 8) | static_cast<uint8_t> (c);
        return true;
    }

    bool integer (Node& node, size_t width, bool sign)
    {
        uint64_t raw;
        if (!number (width, raw))
            return false;
        int shift = static_cast<int> (64 - 8 * width);
        int64_t value = static_cast<int64_t> (raw << shift) >> shift;
        if (sign && value < 0)
        {
            node.kind = Node::Int;
            node.integer = value;
        }
        else
        {
            node.kind = Node::Uint;
            node.uinteger = raw;
        }
        return true;
    }

    bool real (Node& node, size_t width)
    {
        uint64_t raw;
        if (!number (width, raw))
            return false;
        node.kind = Node::Float;
        if (width == 4)
        {
            uint32_t bits = static_cast<uint32_t> (raw);
            float f;
            std::memcpy (&f, &bits, 4);
            node.real = f;
        }
        else
            std::memcpy (&node.real, &raw, 8);
        return true;
    }

    bool data (Node& node, Node::Kind kind, uint64_t size)
    {
        std::string_view raw;
        if (!take (size, raw))
            return false;
        node.kind = kind;
        node.bytes = raw;
        return true;
    }

    bool blob (Node& node, Node::Kind kind, size_t width)
    {
        uint64_t size;
        return number (width, size) && data (node, kind, size);
    }

    bool ext (Node& node, uint64_t size)
    {
        uint64_t type;
        if (!number (1, type))
            return false;
        node.ext_type = static_cast<int8_t> (type);
        return data (node, Node::Ext, size);
    }

    bool array (Node& node, uint64_t count)
    {
        if (count > _in.size () - _pos)
            return false;
        node.kind = Node::Array;
        node.items.resize (count);
        for (auto& item : node.items)
        {
            if (!value (item))
                return false;
        }
        return true;
    }

    bool map (Node& node, uint64_t count)
    {
        if (count > (_in.size () - _pos) / 2)
            return false;
        node.kind = Node::Map;
        node.members.resize (count);
        for (auto& [key, val] : node.members)
        {
            if (!value (key) || !value (val))
                return false;
        }
        return true;
    }

    bool value (Node& node)
    {
        uint64_t head, count;
        if (!number (1, head))
            return false;
        if (head <= 0x7f)
        {
            node.kind = Node::Uint;
            node.uinteger = head;
            return true;
        }
        if (head >= 0xe0)
        {
            node.kind = Node::Int;
            node.integer = static_cast<int8_t> (head);
            return true;
        }
        if (head <= 0x8f)
            return map (node, head & 0x0f);
        if (head <= 0x9f)
            return array (node, head & 0x0f);
        if (head <= 0xbf)
            return data (node, Node::Str, head & 0x1f);
        switch (head)
        {
            case 0xc0:
                node.kind = Node::Nil;
                return true;
            case 0xc2:
            case 0xc3:
                node.kind = Node::Bool;
                node.boolean = head == 0xc3;
                return true;
            case 0xc4:
                return blob (node, Node::Bin, 1);
            case 0xc5:
                return blob (node, Node::Bin, 2);
            case 0xc6:
                return blob (node, Node::Bin, 4);
            case 0xc7:
                return number (1, count) && ext (node, count);
            case 0xc8:
                return number (2, count) && ext (node, count);
            case 0xc9:
                return number (4, count) && ext (node, count);
            case 0xca:
                return real (node, 4);
            case 0xcb:
                return real (node, 8);
            case 0xcc:
                return integer (node, 1, false);
            case 0xcd:
                return integer (node, 2, false);
            case 0xce:
                return integer (node, 4, false);
            case 0xcf:
                return integer (node, 8, false);
            case 0xd0:
                return integer (node, 1, true);
            case 0xd1:
                return integer (node, 2, true);
            case 0xd2:
                return integer (node, 4, true);
            case 0xd3:
                return integer (node, 8, true);
            case 0xd4:
                return ext (node, 1);
            case 0xd5:
                return ext (node, 2);
            case 0xd6:
                return ext (node, 4);
            case 0xd7:
                return ext (node, 8);
            case 0xd8:
                return ext (node, 16);
            case 0xd9:
                return blob (node, Node::Str, 1);
            case 0xda:
                return blob (node, Node::Str, 2);
            case 0xdb:
                return blob (node, Node::Str, 4);
            case 0xdc:
                return number (2, count) && array (node, count);
            case 0xdd:
                return number (4, count) && array (node, count);
            case 0xde:
                return number (2, count) && map (node, count);
            case 0xdf:
                return number (4, count) && map (node, count);
            default:
                return false;
        }
    }

    std::string_view _in;
    size_t _pos = 0;
};

static bool same_value (const Node& a, const Node& b)
{
    if (a.kind != b.kind)
        return false;
    switch (a.kind)
    {
        case Node::Nil:
            return true;
        case Node::Bool:
            return a.boolean == b.boolean;
        case Node::Int:
            return a.integer == b.integer;
        case Node::Uint:
            return a.uinteger == b.uinteger;
        case Node::Float:
            return doubles_bitwise_eq (a.real, b.real);
        case Node::Str:
        case Node::Bin:
            return a.bytes == b.bytes;
        case Node::Ext:
            return a.ext_type == b.ext_type && a.bytes == b.bytes;
        case Node::Array:
            if (a.items.size () != b.items.size ())
                return false;
            for (size_t i = 0; i < a.items.size (); ++i)
            {
                if (!same_value (a.items[i], b.items[i]))
                    return false;
            }
            return true;
        case Node::Map:
            if (a.members.size () != b.members.size ())
                return false;
            for (auto& [key, val] : a.members)
            {
                bool found = false;
                for (size_t j = 0; j < b.members.size () && !found; ++j)
                    found = same_value (key, b.members[j].first) && same_value (val, b.members[j].second);
                if (!found)
                    return false;
            }
            return true;
    }
    return false;
}

static bool same_document (std::string_view expected, std::string_view actual)
{
    auto a = Decoder (expected).decode ();
    auto b = Decoder (actual).decode ();
    return a && b && same_value (*a, *b);
}

template <typename Adapter, typename Output>
std::string_view output_view (const Output& output)
{
    if constexpr (std::is_convertible_v<const Output&, std::string_view>)
        return output;
    else
        return Adapter::view (output);
}

static void count (Score& score, bool ok, bool verbose, const char* what, const std::string& name)
{
    if (ok)
        score.pass++;
    else
    {
        score.fail++;
        if (verbose)
            std::cerr << "  [FAIL] " << what << ": " << name << "\n";
    }
}

template <typename Adapter>
void run_valid (Result& r, const std::vector<Case>& cases, bool verbose)
{
    for (auto& c : cases)
    {
        count (r.accept[c.category], Adapter::accepts (c.bytes), verbose, "should accept", c.name);
        auto doc = Adapter::parse (c.bytes);
        bool same = doc && same_document (c.bytes, output_view<Adapter> (Adapter::serialize (*doc)));
        count (r.roundtrip[c.category], same, verbose, "roundtrip", c.name);
    }
}

template <typename Adapter>
void run_invalid (Result& r, const std::vector<Case>& cases, bool verbose)
{
    for (auto& c : cases)
        count (r.reject, !Adapter::accepts (c.bytes), verbose, "should reject", c.name);
}

template <typename Adapter>
bool run_output_check (const std::vector<std::string>& files, std::vector<std::optional<Stat>>& reference)
{
    bool ok = true;
    auto fail = [&] (const std::string& path, const char* what) {
        ok = false;
        std::cerr << "  [FAIL] " << Adapter::name << " " << what << ": " << fs::path (path).filename ().string ()
                  << "\n";
    };
    for (size_t i = 0; i < files.size (); ++i)
    {
        auto msgpack = load_file (files[i]);
        auto doc = Adapter::parse (msgpack);
        if (!doc)
        {
            fail (files[i], "parse");
            continue;
        }
        auto stat = Adapter::statistics (*doc);
        if (!reference[i])
            reference[i] = stat;
        else if (!(stat == *reference[i]))
            fail (files[i], "statistics");
        auto result = Adapter::serialize (*doc);
        auto output = output_view<Adapter> (result);
        auto reparsed = Adapter::parse (output);
        if (!reparsed || !(Adapter::statistics (*reparsed) == stat) || !same_document (msgpack, output))
            fail (files[i], "serialize");
    }
    return ok;
}

static void print_header ()
{
    std::cout << std::left << std::setw (25) << "Library" << std::right;
    for (const char* label : {"Core", "Core RT", "Bin/Ext", "Bin/Ext RT", "Keys", "Keys RT", "Reject"})
        std::cout << std::setw (12) << label;
    std::cout << "\n" << std::string (109, '-') << "\n";
}

static std::string fmt (const Score& score)
{
    return std::to_string (score.pass) + "/" + std::to_string (score.pass + score.fail);
}

static void print_row (const std::string& name, const Result& r)
{
    std::cout << std::left << std::setw (25) << name << std::right;
    for (int c = Core; c < Malformed; ++c)
        std::cout << std::setw (12) << fmt (r.accept[c]) << std::setw (12) << fmt (r.roundtrip[c]);
    std::cout << std::setw (12) << fmt (r.reject) << "\n";
}

static void write_json (const std::string& output_path, const std::vector<std::pair<std::string, Result>>& all)
{
    std::ofstream out (output_path);
    out << "{\n  \"results\": [\n";
    for (size_t i = 0; i < all.size (); ++i)
    {
        auto& [name, r] = all[i];
        auto kv = [&] (const std::string& key, const Score& score) {
            out << ",\n      \"" << key << "\": {\"pass\": " << score.pass
                << ", \"total\": " << (score.pass + score.fail) << "}";
        };
        out << "    {\n"
            << "      \"library\": \"" << name << "\"";
        for (int c = Core; c < Malformed; ++c)
        {
            kv (std::string (category_keys[c]) + "_accept", r.accept[c]);
            kv (std::string (category_keys[c]) + "_roundtrip", r.roundtrip[c]);
        }
        kv ("reject", r.reject);
        out << "\n    }";
        if (i + 1 < all.size ())
            out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}

int main (int argc, char* argv[])
{
    bool verbose = false;
    std::string output_path;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg (argv[i]);
        if (arg == "-v")
            verbose = true;
        else if (arg == "--output" && i + 1 < argc)
            output_path = argv[++i];
    }

    const fs::path data_dir = DATA_DIR;
    auto valid = valid_cases ();
    auto invalid = invalid_cases ();

    std::vector<std::string> check_files;
    for (const char* name : {"canada.msgpack", "citm_catalog.msgpack", "twitter.msgpack"})
        check_files.push_back ((data_dir / name).string ());
    std::vector<std::optional<Stat>> reference (check_files.size ());
    bool outputs_ok = true;

    std::cout << "\n=== MessagePack Conformance ===\n\n";
    print_header ();

    std::vector<std::pair<std::string, Result>> all_results;

    auto run = [&] (auto adapter) {
        using Adapter = decltype (adapter);
        std::string name{Adapter::name};
        if (verbose)
            std::cout << "\n[" << name << "]\n";

        Result r;
        run_valid<Adapter> (r, valid, verbose);
        run_invalid<Adapter> (r, invalid, verbose);
        outputs_ok &= run_output_check<Adapter> (check_files, reference);
        print_row (name, r);
        all_results.emplace_back (name, r);
    };

    run (JoinAdapter{});
    run (JsonConsAdapter{});
    run (MpackZeroCopyAdapter{});
    run (MsgpackCxxAdapter{});
    run (MsgpackCxxRefAdapter{});
    run (NlohmannAdapter{});
    run (TaoCppAdapter{});

    std::cout << "\n";

    if (!output_path.empty ())
        write_json (output_path, all_results);

    return outputs_ok ? 0 : 1;
}
