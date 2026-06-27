#include "nbt/NbtIo.hpp"

#include <cstring>
#include <stdexcept>

namespace bedrocklua::nbt {

Type Tag::type() const {
    switch (storage_.index()) {
        case 0: return Type::End;
        case 1: return Type::Byte;
        case 2: return Type::Short;
        case 3: return Type::Int;
        case 4: return Type::Long;
        case 5: return Type::Float;
        case 6: return Type::Double;
        case 7: return Type::ByteArray;
        case 8: return Type::String;
        case 9: return Type::List;
        case 10: return Type::Compound;
        case 11: return Type::IntArray;
        case 12: return Type::LongArray;
        default: return Type::End;
    }
}

// --- ZigZag + VarInt -------------------------------------------------------
std::uint32_t zigzagEncode32(std::int32_t v) {
    return (static_cast<std::uint32_t>(v) << 1) ^ static_cast<std::uint32_t>(v >> 31);
}
std::int32_t zigzagDecode32(std::uint32_t v) {
    return static_cast<std::int32_t>(v >> 1) ^ -static_cast<std::int32_t>(v & 1);
}
std::uint64_t zigzagEncode64(std::int64_t v) {
    return (static_cast<std::uint64_t>(v) << 1) ^ static_cast<std::uint64_t>(v >> 63);
}
std::int64_t zigzagDecode64(std::uint64_t v) {
    return static_cast<std::int64_t>(v >> 1) ^ -static_cast<std::int64_t>(v & 1);
}

std::size_t writeVarUInt32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    std::size_t n = 0;
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>(value) | 0x80);
        value >>= 7;
        ++n;
    }
    out.push_back(static_cast<std::uint8_t>(value));
    return n + 1;
}
std::size_t writeVarUInt64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    std::size_t n = 0;
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>(value) | 0x80);
        value >>= 7;
        ++n;
    }
    out.push_back(static_cast<std::uint8_t>(value));
    return n + 1;
}

namespace {
struct NbtError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------
class Writer {
public:
    explicit Writer(Format fmt) : fmt_(fmt) {}
    std::vector<std::uint8_t>& bytes() { return out_; }

    void u8(std::uint8_t v) { out_.push_back(v); }
    void i16le(std::int16_t v) {
        auto u = static_cast<std::uint16_t>(v);
        out_.push_back(u & 0xFF);
        out_.push_back((u >> 8) & 0xFF);
    }
    void i32le(std::int32_t v) {
        auto u = static_cast<std::uint32_t>(v);
        for (int i = 0; i < 4; ++i) out_.push_back((u >> (8 * i)) & 0xFF);
    }
    void i64le(std::int64_t v) {
        auto u = static_cast<std::uint64_t>(v);
        for (int i = 0; i < 8; ++i) out_.push_back((u >> (8 * i)) & 0xFF);
    }
    void f32(float v) {
        std::uint32_t u;
        std::memcpy(&u, &v, 4);
        for (int i = 0; i < 4; ++i) out_.push_back((u >> (8 * i)) & 0xFF);
    }
    void f64(double v) {
        std::uint64_t u;
        std::memcpy(&u, &v, 8);
        for (int i = 0; i < 8; ++i) out_.push_back((u >> (8 * i)) & 0xFF);
    }

    void writeInt(std::int32_t v) {
        if (fmt_ == Format::Network) writeVarUInt32(out_, zigzagEncode32(v));
        else i32le(v);
    }
    void writeLong(std::int64_t v) {
        if (fmt_ == Format::Network) writeVarUInt64(out_, zigzagEncode64(v));
        else i64le(v);
    }
    void writeString(const std::string& s) {
        if (fmt_ == Format::Network) {
            writeVarUInt32(out_, static_cast<std::uint32_t>(s.size()));
        } else {
            i16le(static_cast<std::int16_t>(s.size()));
        }
        out_.insert(out_.end(), s.begin(), s.end());
    }
    void writeLen(std::size_t n) { writeInt(static_cast<std::int32_t>(n)); }

    void payload(const Tag& tag) {
        switch (tag.type()) {
            case Type::Byte: u8(static_cast<std::uint8_t>(std::get<std::int8_t>(tag.storage()))); break;
            case Type::Short: i16le(std::get<std::int16_t>(tag.storage())); break;
            case Type::Int: writeInt(std::get<std::int32_t>(tag.storage())); break;
            case Type::Long: writeLong(std::get<std::int64_t>(tag.storage())); break;
            case Type::Float: f32(std::get<float>(tag.storage())); break;
            case Type::Double: f64(std::get<double>(tag.storage())); break;
            case Type::ByteArray: {
                const auto& a = std::get<std::vector<std::int8_t>>(tag.storage());
                writeLen(a.size());
                for (auto b : a) u8(static_cast<std::uint8_t>(b));
                break;
            }
            case Type::String: writeString(std::get<std::string>(tag.storage())); break;
            case Type::List: {
                const auto& list = tag.asList();
                Type elemType = list.empty() ? Type::End : list.front().type();
                u8(static_cast<std::uint8_t>(elemType));
                writeLen(list.size());
                for (const auto& e : list) {
                    if (e.type() != elemType) {
                        throw NbtError("heterogeneous list element type");
                    }
                    payload(e);
                }
                break;
            }
            case Type::Compound: {
                for (const auto& [name, value] : tag.asCompound()) {
                    u8(static_cast<std::uint8_t>(value.type()));
                    writeString(name);
                    payload(value);
                }
                u8(static_cast<std::uint8_t>(Type::End));
                break;
            }
            case Type::IntArray: {
                const auto& a = std::get<std::vector<std::int32_t>>(tag.storage());
                writeLen(a.size());
                for (auto v : a) writeInt(v);
                break;
            }
            case Type::LongArray: {
                const auto& a = std::get<std::vector<std::int64_t>>(tag.storage());
                writeLen(a.size());
                for (auto v : a) writeLong(v);
                break;
            }
            case Type::End: break;
        }
    }

private:
    Format fmt_;
    std::vector<std::uint8_t> out_;
};

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------
class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size, Format fmt)
        : data_(data), size_(size), fmt_(fmt) {}

    std::uint8_t u8() {
        need(1);
        return data_[pos_++];
    }
    std::int16_t i16le() {
        need(2);
        std::uint16_t u = data_[pos_] | (std::uint16_t(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return static_cast<std::int16_t>(u);
    }
    std::int32_t i32le() {
        need(4);
        std::uint32_t u = 0;
        for (int i = 0; i < 4; ++i) u |= std::uint32_t(data_[pos_ + i]) << (8 * i);
        pos_ += 4;
        return static_cast<std::int32_t>(u);
    }
    std::int64_t i64le() {
        need(8);
        std::uint64_t u = 0;
        for (int i = 0; i < 8; ++i) u |= std::uint64_t(data_[pos_ + i]) << (8 * i);
        pos_ += 8;
        return static_cast<std::int64_t>(u);
    }
    float f32() {
        std::uint32_t u = static_cast<std::uint32_t>(i32le());
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    double f64() {
        std::uint64_t u = static_cast<std::uint64_t>(i64le());
        double d;
        std::memcpy(&d, &u, 8);
        return d;
    }
    std::uint32_t varU32() {
        std::uint32_t result = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            std::uint8_t b = u8();
            result |= std::uint32_t(b & 0x7F) << shift;
            if ((b & 0x80) == 0) return result;
        }
        throw NbtError("VarInt32 too long");
    }
    std::uint64_t varU64() {
        std::uint64_t result = 0;
        for (int shift = 0; shift < 70; shift += 7) {
            std::uint8_t b = u8();
            result |= std::uint64_t(b & 0x7F) << shift;
            if ((b & 0x80) == 0) return result;
        }
        throw NbtError("VarInt64 too long");
    }

    std::int32_t readInt() {
        return fmt_ == Format::Network ? zigzagDecode32(varU32()) : i32le();
    }
    std::int64_t readLong() {
        return fmt_ == Format::Network ? zigzagDecode64(varU64()) : i64le();
    }
    std::string readString() {
        std::size_t len = fmt_ == Format::Network ? varU32()
                                                  : static_cast<std::uint16_t>(i16le());
        need(len);
        std::string s(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return s;
    }
    std::size_t readLen() {
        std::int32_t n = readInt();
        if (n < 0) throw NbtError("negative length");
        if (static_cast<std::size_t>(n) > size_ - pos_ + 1) {
            // Loose upper bound; exact bounds enforced by need() during reads.
        }
        return static_cast<std::size_t>(n);
    }

    Tag payload(Type type) {
        switch (type) {
            case Type::Byte: return Tag(static_cast<std::int8_t>(u8()));
            case Type::Short: return Tag(i16le());
            case Type::Int: return Tag(readInt());
            case Type::Long: return Tag(readLong());
            case Type::Float: return Tag(f32());
            case Type::Double: return Tag(f64());
            case Type::ByteArray: {
                std::size_t n = readLen();
                std::vector<std::int8_t> a(n);
                for (std::size_t i = 0; i < n; ++i) a[i] = static_cast<std::int8_t>(u8());
                return Tag(std::move(a));
            }
            case Type::String: return Tag(readString());
            case Type::List: {
                auto elemType = static_cast<Type>(u8());
                std::size_t n = readLen();
                ListData list;
                list.reserve(n);
                for (std::size_t i = 0; i < n; ++i) list.push_back(payload(elemType));
                return Tag(std::move(list));
            }
            case Type::Compound: return Tag(readCompound());
            case Type::IntArray: {
                std::size_t n = readLen();
                std::vector<std::int32_t> a(n);
                for (std::size_t i = 0; i < n; ++i) a[i] = readInt();
                return Tag(std::move(a));
            }
            case Type::LongArray: {
                std::size_t n = readLen();
                std::vector<std::int64_t> a(n);
                for (std::size_t i = 0; i < n; ++i) a[i] = readLong();
                return Tag(std::move(a));
            }
            case Type::End: return Tag();
        }
        throw NbtError("unknown tag type " + std::to_string(static_cast<int>(type)));
    }

    CompoundData readCompound() {
        CompoundData out;
        while (true) {
            auto type = static_cast<Type>(u8());
            if (type == Type::End) break;
            std::string name = readString();
            out.emplace(std::move(name), payload(type));
        }
        return out;
    }

    std::size_t pos() const { return pos_; }

private:
    void need(std::size_t n) {
        if (pos_ + n > size_) throw NbtError("unexpected end of NBT data");
    }
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    Format fmt_;
};
}  // namespace

Result<std::vector<std::uint8_t>> write(const Tag& root, Format format,
                                        const std::string& rootName) {
    if (!root.isCompound()) {
        return Result<std::vector<std::uint8_t>>::fail("NBT root must be a compound");
    }
    try {
        Writer w(format);
        w.u8(static_cast<std::uint8_t>(Type::Compound));
        w.writeString(rootName);
        w.payload(root);
        return Result<std::vector<std::uint8_t>>(std::move(w.bytes()));
    } catch (const std::exception& e) {
        return Result<std::vector<std::uint8_t>>::fail(std::string("NBT write failed: ") + e.what());
    }
}

Result<Tag> read(const std::uint8_t* data, std::size_t size, Format format) {
    try {
        Reader r(data, size, format);
        auto type = static_cast<Type>(r.u8());
        if (type != Type::Compound) {
            return Result<Tag>::fail("NBT root is not a compound");
        }
        (void)r.readString();  // root name (usually empty)
        return Result<Tag>(Tag(r.readCompound()));
    } catch (const std::exception& e) {
        return Result<Tag>::fail(std::string("NBT read failed: ") + e.what());
    }
}

}  // namespace bedrocklua::nbt
