#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <msgpack.hpp>

#include "common/stat.hpp"

namespace msgpack_cxx_detail
{

    inline void gen_stat (Stat& stat, const msgpack::object& v)
    {
        switch (v.type)
        {
            case msgpack::type::MAP:
                stat.object_count++;
                for (uint32_t i = 0; i < v.via.map.size; ++i)
                {
                    ++stat.member_count;
                    gen_stat (stat, v.via.map.ptr[i].key);
                    gen_stat (stat, v.via.map.ptr[i].val);
                }
                break;

            case msgpack::type::ARRAY:
                stat.array_count++;
                for (uint32_t i = 0; i < v.via.array.size; ++i)
                {
                    ++stat.element_count;
                    gen_stat (stat, v.via.array.ptr[i]);
                }
                break;

            case msgpack::type::STR:
                stat.string_count++;
                stat.string_length += v.via.str.size;
                break;

            case msgpack::type::POSITIVE_INTEGER:
            case msgpack::type::NEGATIVE_INTEGER:
            case msgpack::type::FLOAT32:
            case msgpack::type::FLOAT64:
                stat.number_count++;
                break;

            case msgpack::type::BOOLEAN:
                if (v.via.boolean)
                    stat.true_count++;
                else
                    stat.false_count++;
                break;

            case msgpack::type::NIL:
                stat.null_count++;
                break;

            default:
                break;
        }
    }

}

template <bool ZeroCopy>
struct MsgpackCxxBase
{
    using Document = msgpack::object_handle;

    static bool reference (msgpack::type::object_type type, std::size_t, void*)
    {
        return type == msgpack::type::STR || type == msgpack::type::BIN || type == msgpack::type::EXT;
    }

    static std::optional<Document> parse (std::string_view msgpack)
    {
        try
        {
            std::size_t offset = 0;
            bool referenced = false;
            Document doc =
                msgpack::unpack (msgpack.data (), msgpack.size (), offset, referenced, ZeroCopy ? reference : nullptr);
            if (offset != msgpack.size ())
                return std::nullopt;
            return doc;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    static msgpack::sbuffer serialize (const Document& doc)
    {
        msgpack::sbuffer sbuf;
        msgpack::pack (sbuf, doc.get ());
        return sbuf;
    }

    static std::string_view view (const msgpack::sbuffer& sbuf)
    {
        return {sbuf.data (), sbuf.size ()};
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        msgpack_cxx_detail::gen_stat (stat, doc.get ());
        return stat;
    }

    static bool accepts (std::string_view msgpack)
    {
        return parse (msgpack).has_value ();
    }
};

struct MsgpackCxxAdapter : MsgpackCxxBase<false>
{
    static constexpr std::string_view name = "msgpack-cxx";
};

struct MsgpackCxxRefAdapter : MsgpackCxxBase<true>
{
    static constexpr std::string_view name = "msgpack-cxx/zero-copy";
};
