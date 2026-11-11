#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <streambuf>
#include <string>
#include <string_view>

#include <join/pack.hpp>

#include "common/stat.hpp"

struct JoinOutput
{
    struct FreeDeleter
    {
        void operator() (char* data) const noexcept
        {
            std::free (data);
        }
    };

    std::unique_ptr<char, FreeDeleter> data;
    size_t size = 0;
};

struct OutputStreambuf : std::streambuf
{
    JoinOutput out;
    size_t capacity = 0;

    explicit OutputStreambuf (size_t reserve = 8192)
    {
        grow (reserve);
    }

    void grow (size_t n)
    {
        if (out.size + n <= capacity)
            return;
        size_t cap = std::max (capacity * 2, out.size + n);
        char* data = static_cast<char*> (std::realloc (out.data.get (), cap));
        if (data == nullptr)
            throw std::bad_alloc ();
        out.data.release ();
        out.data.reset (data);
        capacity = cap;
    }

    int_type overflow (int_type c) override
    {
        if (c != EOF)
        {
            grow (1);
            out.data.get ()[out.size++] = static_cast<char> (c);
        }
        return c;
    }

    std::streamsize xsputn (const char* s, std::streamsize n) override
    {
        grow (static_cast<size_t> (n));
        std::memcpy (out.data.get () + out.size, s, static_cast<size_t> (n));
        out.size += static_cast<size_t> (n);
        return n;
    }
};

namespace join_detail
{

    inline void gen_stat (Stat& stat, const join::Value& v)
    {
        switch (v.index ())
        {
            case join::Value::ArrayValue:
                stat.array_count++;
                for (auto& elem : v.getArray ())
                {
                    ++stat.element_count;
                    gen_stat (stat, elem);
                }
                break;

            case join::Value::ObjectValue:
                stat.object_count++;
                for (auto& [key, val] : v.getObject ())
                {
                    ++stat.member_count;
                    gen_stat (stat, key);
                    gen_stat (stat, val);
                }
                break;

            case join::Value::StringValue:
                stat.string_count++;
                stat.string_length += v.size ();
                break;

            case join::Value::IntegerValue:
            case join::Value::UnsignedValue:
            case join::Value::Integer64Value:
            case join::Value::Unsigned64Value:
            case join::Value::RealValue:
                stat.number_count++;
                break;

            case join::Value::BooleanValue:
                if (v.isTrue ())
                    stat.true_count++;
                else
                    stat.false_count++;
                break;

            case join::Value::NullValue:
                stat.null_count++;
                break;
        }
    }

}

struct JoinAdapter
{
    static constexpr std::string_view name = "join";

    using Document = join::Value;

    static std::optional<Document> parse (std::string_view msgpack)
    {
        Document doc;
        if (doc.deserialize<join::PackReader> (msgpack.data (), msgpack.size ()) == -1)
            return std::nullopt;
        return doc;
    }

    static JoinOutput serialize (const Document& doc)
    {
        OutputStreambuf buf;
        std::ostream os (&buf);
        doc.serialize<join::PackWriter> (os);
        return std::move (buf.out);
    }

    static std::string_view view (const JoinOutput& out)
    {
        return {out.data.get (), out.size};
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        join_detail::gen_stat (stat, doc);
        return stat;
    }

    static bool accepts (std::string_view msgpack)
    {
        Document doc;
        return doc.deserialize<join::PackReader> (msgpack.data (), msgpack.size ()) != -1;
    }
};
