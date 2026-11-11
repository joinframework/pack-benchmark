#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <mpack/mpack.h>

#include "common/stat.hpp"

namespace mpack_detail
{

    struct TreeDeleter
    {
        void operator() (mpack_tree_t* tree) const noexcept
        {
            mpack_tree_destroy (tree);
            delete tree;
        }
    };

    inline void gen_stat (Stat& stat, mpack_node_t v)
    {
        switch (mpack_node_type (v))
        {
            case mpack_type_map:
                {
                    stat.object_count++;
                    size_t count = mpack_node_map_count (v);
                    for (size_t i = 0; i < count; ++i)
                    {
                        ++stat.member_count;
                        gen_stat (stat, mpack_node_map_key_at (v, i));
                        gen_stat (stat, mpack_node_map_value_at (v, i));
                    }
                    break;
                }
            case mpack_type_array:
                {
                    stat.array_count++;
                    size_t length = mpack_node_array_length (v);
                    for (size_t i = 0; i < length; ++i)
                    {
                        ++stat.element_count;
                        gen_stat (stat, mpack_node_array_at (v, i));
                    }
                    break;
                }
            case mpack_type_str:
                stat.string_count++;
                stat.string_length += mpack_node_strlen (v);
                break;

            case mpack_type_int:
            case mpack_type_uint:
            case mpack_type_float:
            case mpack_type_double:
                stat.number_count++;
                break;

            case mpack_type_bool:
                if (mpack_node_bool (v))
                    stat.true_count++;
                else
                    stat.false_count++;
                break;

            case mpack_type_nil:
                stat.null_count++;
                break;

            default:
                break;
        }
    }

    inline void write (mpack_writer_t* writer, mpack_node_t v)
    {
        switch (mpack_node_type (v))
        {
            case mpack_type_map:
                {
                    size_t count = mpack_node_map_count (v);
                    mpack_start_map (writer, static_cast<uint32_t> (count));
                    for (size_t i = 0; i < count; ++i)
                    {
                        write (writer, mpack_node_map_key_at (v, i));
                        write (writer, mpack_node_map_value_at (v, i));
                    }
                    mpack_finish_map (writer);
                    break;
                }
            case mpack_type_array:
                {
                    size_t length = mpack_node_array_length (v);
                    mpack_start_array (writer, static_cast<uint32_t> (length));
                    for (size_t i = 0; i < length; ++i)
                        write (writer, mpack_node_array_at (v, i));
                    mpack_finish_array (writer);
                    break;
                }
            case mpack_type_str:
                mpack_write_str (writer, mpack_node_str (v), static_cast<uint32_t> (mpack_node_strlen (v)));
                break;

            case mpack_type_bin:
                mpack_write_bin (writer, mpack_node_bin_data (v), static_cast<uint32_t> (mpack_node_bin_size (v)));
                break;

            case mpack_type_ext:
                mpack_write_ext (writer, mpack_node_exttype (v), mpack_node_data (v), mpack_node_data_len (v));
                break;

            case mpack_type_int:
                mpack_write_i64 (writer, mpack_node_i64 (v));
                break;

            case mpack_type_uint:
                mpack_write_u64 (writer, mpack_node_u64 (v));
                break;

            case mpack_type_float:
                mpack_write_float (writer, mpack_node_float_strict (v));
                break;

            case mpack_type_double:
                mpack_write_double (writer, mpack_node_double_strict (v));
                break;

            case mpack_type_bool:
                mpack_write_bool (writer, mpack_node_bool (v));
                break;

            case mpack_type_nil:
                mpack_write_nil (writer);
                break;

            default:
                break;
        }
    }

    struct FreeDeleter
    {
        void operator() (char* data) const noexcept
        {
            std::free (data);
        }
    };

    struct Output
    {
        std::unique_ptr<char, FreeDeleter> data;
        size_t size = 0;
    };

}

struct MpackZeroCopyAdapter
{
    static constexpr std::string_view name = "mpack/zero-copy";

    using Document = std::unique_ptr<mpack_tree_t, mpack_detail::TreeDeleter>;

    static std::optional<Document> parse (std::string_view msgpack)
    {
        Document doc (new mpack_tree_t);
        mpack_tree_init_data (doc.get (), msgpack.data (), msgpack.size ());
        mpack_tree_parse (doc.get ());
        if (mpack_tree_error (doc.get ()) != mpack_ok || mpack_tree_size (doc.get ()) != msgpack.size ())
            return std::nullopt;
        return doc;
    }

    static mpack_detail::Output serialize (const Document& doc)
    {
        char* data = nullptr;
        size_t size = 0;
        mpack_writer_t writer;
        mpack_writer_init_growable (&writer, &data, &size);
        mpack_detail::write (&writer, mpack_tree_root (doc.get ()));
        mpack_detail::Output out;
        if (mpack_writer_destroy (&writer) == mpack_ok)
        {
            out.data.reset (data);
            out.size = size;
        }
        return out;
    }

    static std::string_view view (const mpack_detail::Output& out)
    {
        return {out.data.get (), out.size};
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        mpack_detail::gen_stat (stat, mpack_tree_root (doc.get ()));
        return stat;
    }

    static bool accepts (std::string_view msgpack)
    {
        return parse (msgpack).has_value ();
    }
};
