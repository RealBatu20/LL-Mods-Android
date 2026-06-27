#pragma once

// NbtIo - Minecraft Bedrock NBT read/write.
//
// Supports the two encodings Bedrock actually uses:
//   * LittleEndian  - level.dat / .mcstructure on disk (fixed-width little-endian)
//   * Network       - over-the-wire / packet NBT (VarInt + ZigZag for int/long,
//                     unsigned VarInt string + name lengths)
//
// A Tag is a tagged union over every NBT type (compound, list, the scalar
// types, and the byte/int/long arrays). The model is value-based so scripts can
// build, mutate, and re-serialize world/item/entity data safely.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "util/Result.hpp"

namespace bedrocklua::nbt {

enum class Type : std::uint8_t {
    End = 0,
    Byte = 1,
    Short = 2,
    Int = 3,
    Long = 4,
    Float = 5,
    Double = 6,
    ByteArray = 7,
    String = 8,
    List = 9,
    Compound = 10,
    IntArray = 11,
    LongArray = 12,
};

enum class Format {
    LittleEndian,  // disk format
    Network,       // VarInt / ZigZag packet format
};

class Tag;
using CompoundData = std::map<std::string, Tag>;
using ListData = std::vector<Tag>;

class Tag {
public:
    using Storage = std::variant<std::monostate,             // End
                                 std::int8_t,                // Byte
                                 std::int16_t,               // Short
                                 std::int32_t,               // Int
                                 std::int64_t,               // Long
                                 float,                      // Float
                                 double,                     // Double
                                 std::vector<std::int8_t>,   // ByteArray
                                 std::string,                // String
                                 ListData,                   // List
                                 CompoundData,               // Compound
                                 std::vector<std::int32_t>,  // IntArray
                                 std::vector<std::int64_t>>; // LongArray

    Tag() = default;
    Tag(std::int8_t v) : storage_(v) {}
    Tag(std::int16_t v) : storage_(v) {}
    Tag(std::int32_t v) : storage_(v) {}
    Tag(std::int64_t v) : storage_(v) {}
    Tag(float v) : storage_(v) {}
    Tag(double v) : storage_(v) {}
    Tag(std::vector<std::int8_t> v) : storage_(std::move(v)) {}
    Tag(std::string v) : storage_(std::move(v)) {}
    Tag(ListData v) : storage_(std::move(v)) {}
    Tag(CompoundData v) : storage_(std::move(v)) {}
    Tag(std::vector<std::int32_t> v) : storage_(std::move(v)) {}
    Tag(std::vector<std::int64_t> v) : storage_(std::move(v)) {}

    Type type() const;
    const Storage& storage() const { return storage_; }
    Storage& storage() { return storage_; }

    bool isCompound() const { return std::holds_alternative<CompoundData>(storage_); }
    bool isList() const { return std::holds_alternative<ListData>(storage_); }

    CompoundData& asCompound() { return std::get<CompoundData>(storage_); }
    const CompoundData& asCompound() const { return std::get<CompoundData>(storage_); }
    ListData& asList() { return std::get<ListData>(storage_); }
    const ListData& asList() const { return std::get<ListData>(storage_); }

    static Tag makeCompound() { return Tag(CompoundData{}); }
    static Tag makeList() { return Tag(ListData{}); }

private:
    Storage storage_;
};

// Serialize a *named-root* compound (the Bedrock convention: the root is a
// compound with a name, usually empty). Returns the byte buffer.
Result<std::vector<std::uint8_t>> write(const Tag& root, Format format,
                                        const std::string& rootName = "");

// Parse a named-root compound from a buffer.
Result<Tag> read(const std::uint8_t* data, std::size_t size, Format format);

inline Result<Tag> read(const std::vector<std::uint8_t>& buf, Format format) {
    return read(buf.data(), buf.size(), format);
}

// VarInt / ZigZag primitives (exposed for the network-data bindings and tests).
std::size_t writeVarUInt32(std::vector<std::uint8_t>& out, std::uint32_t value);
std::size_t writeVarUInt64(std::vector<std::uint8_t>& out, std::uint64_t value);
std::uint32_t zigzagEncode32(std::int32_t v);
std::int32_t zigzagDecode32(std::uint32_t v);
std::uint64_t zigzagEncode64(std::int64_t v);
std::int64_t zigzagDecode64(std::uint64_t v);

}  // namespace bedrocklua::nbt
