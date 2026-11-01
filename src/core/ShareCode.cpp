#include "core/ShareCode.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <vector>

namespace radiance::sharecode
{
    namespace
    {
        constexpr std::string_view kPrefix = "RD-";
        constexpr std::string_view kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";  // Crockford base32
        constexpr int kVersion = 1;

        // Each field is stored as (value - minimum) in a fixed number of bits.
        struct Field
        {
            int ColourLook::*member;
            int minimum;
            int bits;
        };

        constexpr std::array<Field, 16> kFields{ {
            { &ColourLook::saturation, 0, 9 },
            { &ColourLook::vibrance, 0, 9 },
            { &ColourLook::brightness, 50, 7 },
            { &ColourLook::gamma, 50, 7 },
            { &ColourLook::contrast, 50, 7 },
            { &ColourLook::temperature, -100, 8 },
            { &ColourLook::highlights, -100, 8 },
            { &ColourLook::shadows, -100, 8 },
            { &ColourLook::blacks, -100, 8 },
            { &ColourLook::whites, -100, 8 },
            { &ColourLook::shadowTint, -100, 8 },
            { &ColourLook::highlightTint, -100, 8 },
            { &ColourLook::hue, -180, 9 },
            { &ColourLook::redGain, 50, 7 },
            { &ColourLook::greenGain, 50, 7 },
            { &ColourLook::blueGain, 50, 7 },
        } };

        class BitWriter
        {
        public:
            void Put(uint32_t value, int bits)
            {
                for (int i = bits - 1; i >= 0; --i)
                    bits_.push_back(((value >> i) & 1u) != 0);
            }
            const std::vector<bool>& Bits() const { return bits_; }

        private:
            std::vector<bool> bits_;
        };

        class BitReader
        {
        public:
            explicit BitReader(const std::vector<bool>& bits) : bits_(bits) {}
            bool Get(int bits, uint32_t& out)
            {
                out = 0;
                for (int i = 0; i < bits; ++i)
                {
                    if (pos_ >= bits_.size())
                        return false;
                    out = (out << 1) | (bits_[pos_++] ? 1u : 0u);
                }
                return true;
            }

        private:
            const std::vector<bool>& bits_;
            size_t pos_ = 0;
        };

        uint8_t Checksum(const std::vector<bool>& bits, size_t count)
        {
            uint8_t sum = 0x5A;
            for (size_t i = 0; i < count; ++i)
                sum = static_cast<uint8_t>((sum << 1 | sum >> 7) ^ (bits[i] ? 0x1D : 0x00) ^ static_cast<uint8_t>(i));
            return sum;
        }

        constexpr int PayloadBits()
        {
            int bits = 4 + 2;  // version + colour vision
            for (const Field& f : kFields)
                bits += f.bits;
            return bits;
        }
    }

    std::string Encode(const ColourLook& look)
    {
        BitWriter w;
        w.Put(kVersion, 4);
        for (const Field& f : kFields)
        {
            const int raw = std::clamp(look.*(f.member) - f.minimum, 0, (1 << f.bits) - 1);
            w.Put(static_cast<uint32_t>(raw), f.bits);
        }
        w.Put(static_cast<uint32_t>(look.vision), 2);
        w.Put(Checksum(w.Bits(), w.Bits().size()), 8);
        std::vector<bool> bits = w.Bits();
        while (bits.size() % 5)
            bits.push_back(false);

        std::string code(kPrefix);
        for (size_t i = 0; i < bits.size(); i += 5)
        {
            int v = 0;
            for (int b = 0; b < 5; ++b)
                v = (v << 1) | (bits[i + b] ? 1 : 0);
            code += kAlphabet[static_cast<size_t>(v)];
        }
        return code;
    }

    std::optional<ColourLook> Decode(std::string_view code)
    {
        std::string clean;
        for (const char ch : code)
        {
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '-')
                continue;
            clean += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        if (clean.rfind("RD", 0) != 0)
            return std::nullopt;
        clean.erase(0, 2);

        std::vector<bool> bits;
        for (char ch : clean)
        {
            // Crockford: O reads as 0, I and L read as 1.
            if (ch == 'O') ch = '0';
            if (ch == 'I' || ch == 'L') ch = '1';
            const size_t v = kAlphabet.find(ch);
            if (v == std::string_view::npos)
                return std::nullopt;
            for (int b = 4; b >= 0; --b)
                bits.push_back(((v >> b) & 1u) != 0);
        }
        if (bits.size() < static_cast<size_t>(PayloadBits() + 8))
            return std::nullopt;

        BitReader r(bits);
        uint32_t version = 0;
        if (!r.Get(4, version) || version != kVersion)
            return std::nullopt;
        ColourLook look;
        for (const Field& f : kFields)
        {
            uint32_t raw = 0;
            if (!r.Get(f.bits, raw))
                return std::nullopt;
            look.*(f.member) = static_cast<int>(raw) + f.minimum;
        }
        uint32_t vision = 0, sum = 0;
        if (!r.Get(2, vision) || !r.Get(8, sum))
            return std::nullopt;
        if (sum != Checksum(bits, static_cast<size_t>(PayloadBits())))
            return std::nullopt;
        look.vision = static_cast<ColourVision>(vision);

        // Anything outside the sliders' ranges means the code was not one of ours.
        const bool inRange = look.saturation <= 300 && look.vibrance <= 350 && look.brightness <= 170 && look.gamma <= 150 &&
            look.contrast <= 150 && look.temperature <= 100 && look.highlights <= 100 && look.shadows <= 100 && look.blacks <= 100 &&
            look.whites <= 100 && look.shadowTint <= 100 && look.highlightTint <= 100 && look.hue <= 180 && look.redGain <= 150 &&
            look.greenGain <= 150 && look.blueGain <= 150;
        if (!inRange)
            return std::nullopt;
        return look;
    }
}
