#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <tao/json.hpp>
#include <tao/json/msgpack.hpp>

#include "common/stat.hpp"

namespace taocpp_detail
{

    inline void gen_stat (Stat& stat, const tao::json::value& v)
    {
        switch (v.type ())
        {
            case tao::json::type::OBJECT:
                stat.object_count++;
                for (auto& [key, val] : v.get_object ())
                {
                    ++stat.member_count;
                    ++stat.string_count;
                    stat.string_length += key.size ();
                    gen_stat (stat, val);
                }
                break;

            case tao::json::type::ARRAY:
                stat.array_count++;
                for (auto& elem : v.get_array ())
                {
                    ++stat.element_count;
                    gen_stat (stat, elem);
                }
                break;

            case tao::json::type::STRING:
                stat.string_count++;
                stat.string_length += v.get_string ().size ();
                break;

            case tao::json::type::STRING_VIEW:
                stat.string_count++;
                stat.string_length += v.get_string_view ().size ();
                break;

            case tao::json::type::SIGNED:
            case tao::json::type::UNSIGNED:
            case tao::json::type::DOUBLE:
                stat.number_count++;
                break;

            case tao::json::type::BOOLEAN:
                if (v.get_boolean ())
                    stat.true_count++;
                else
                    stat.false_count++;
                break;

            case tao::json::type::NULL_:
                stat.null_count++;
                break;

            default:
                break;
        }
    }

}

struct TaoCppAdapter
{
    static constexpr std::string_view name = "taocpp";

    using Document = tao::json::value;

    static std::optional<Document> parse (std::string_view msgpack)
    {
        try
        {
            return tao::json::msgpack::from_string (msgpack);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    static std::string serialize (const Document& doc)
    {
        return tao::json::msgpack::to_string (doc);
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        taocpp_detail::gen_stat (stat, doc);
        return stat;
    }

    static bool accepts (std::string_view msgpack)
    {
        return parse (msgpack).has_value ();
    }
};
