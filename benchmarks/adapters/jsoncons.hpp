#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <jsoncons/json.hpp>
#include <jsoncons_ext/msgpack/msgpack.hpp>

#include "common/stat.hpp"

namespace jsoncons_detail
{

    inline void gen_stat (Stat& stat, const jsoncons::json& v)
    {
        if (v.is_object ())
        {
            stat.object_count++;
            for (auto& kv : v.object_range ())
            {
                ++stat.member_count;
                ++stat.string_count;
                stat.string_length += kv.key ().size ();
                gen_stat (stat, kv.value ());
            }
        }
        else if (v.is_array ())
        {
            stat.array_count++;
            for (auto& elem : v.array_range ())
            {
                ++stat.element_count;
                gen_stat (stat, elem);
            }
        }
        else if (v.is_number ())
        {
            stat.number_count++;
        }
        else if (v.is_string ())
        {
            stat.string_count++;
            stat.string_length += v.as_string_view ().size ();
        }
        else if (v.is_bool ())
        {
            if (v.as<bool> ())
                stat.true_count++;
            else
                stat.false_count++;
        }
        else if (v.is_null ())
        {
            stat.null_count++;
        }
    }

}

struct JsonConsAdapter
{
    static constexpr std::string_view name = "jsoncons";

    using Document = jsoncons::json;

    static std::optional<Document> parse (std::string_view msgpack)
    {
        try
        {
            jsoncons::json_decoder<Document> decoder;
            jsoncons::msgpack::msgpack_bytes_reader reader (msgpack, decoder);
            std::error_code ec;
            reader.read (ec);
            if (ec || !decoder.is_valid () || reader.column () != msgpack.size ())
                return std::nullopt;
            return decoder.get_result ();
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    static std::string serialize (const Document& doc)
    {
        std::string out;
        jsoncons::msgpack::encode_msgpack (doc, out);
        return out;
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        jsoncons_detail::gen_stat (stat, doc);
        return stat;
    }

    static bool accepts (std::string_view msgpack)
    {
        return parse (msgpack).has_value ();
    }
};
