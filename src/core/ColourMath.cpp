#include "core/ColourMath.h"

#include <algorithm>
#include <cmath>

namespace radiance::colour
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        uint16_t Quantise(double v)
        {
            return static_cast<uint16_t>(std::clamp(std::round(std::clamp(v, 0.0, 1.0) * 65535.0), 0.0, 65535.0));
        }

        double Norm(int slider)
        {
            return std::clamp(slider, -100, 100) / 100.0;
        }

        double Weight(double v, int sharpness)
        {
            return sharpness == 2 ? v * v : std::pow(std::max(0.0, v), sharpness);
        }

        void MakeMonotonic(Ramp& ramp)
        {
            for (int c = 0; c < 3; ++c)
            {
                for (int i = 1; i < 256; ++i)
                {
                    uint16_t& v = ramp[static_cast<size_t>(c * 256 + i)];
                    v = std::max(v, ramp[static_cast<size_t>(c * 256 + i - 1)]);
                }
            }
        }

        // Saturation around Rec. 709 luminance, per output channel.
        Matrix SaturationMatrix(float sR, float sG, float sB)
        {
            constexpr float lr = 0.2126f, lg = 0.7152f, lb = 0.0722f;
            Matrix m{};
            m[0] = lr * (1 - sR) + sR; m[1] = lr * (1 - sG);      m[2] = lr * (1 - sB);
            m[5] = lg * (1 - sR);      m[6] = lg * (1 - sG) + sG; m[7] = lg * (1 - sB);
            m[10] = lb * (1 - sR);     m[11] = lb * (1 - sG);     m[12] = lb * (1 - sB) + sB;
            m[18] = 1.0f;
            m[24] = 1.0f;
            return m;
        }

        Matrix HueMatrix(int degrees)
        {
            const double a = std::clamp(degrees, -180, 180) * kPi / 180.0;
            const double c = std::cos(a);
            const double k = (1.0 - c) / 3.0;
            const double s = std::sin(a) / std::sqrt(3.0);
            static constexpr int kCross[3][3] = { { 0, -1, 1 }, { 1, 0, -1 }, { -1, 1, 0 } };
            Matrix m{};
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                    m[static_cast<size_t>(i * 5 + j)] = static_cast<float>((i == j ? c : 0.0) + k + s * kCross[j][i]);
            }
            m[18] = 1.0f;
            m[24] = 1.0f;
            return m;
        }

        using M3 = std::array<std::array<double, 3>, 3>;

        M3 Mul3(const M3& a, const M3& b)
        {
            M3 r{};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    for (int k = 0; k < 3; ++k)
                        r[i][j] += a[i][k] * b[k][j];
            return r;
        }

        // Daltonisation: simulate the deficiency in LMS space, then shift the colour
        // information the eye loses into channels it can still see.
        Matrix VisionMatrix(ColourVision mode)
        {
            Matrix m = IdentityMatrix();
            if (mode == ColourVision::Normal)
                return m;
            static constexpr M3 kRgbToLms{ { { 17.8824, 43.5161, 4.11935 }, { 3.45565, 27.1554, 3.86714 }, { 0.0299566, 0.184309, 1.46709 } } };
            static constexpr M3 kLmsToRgb{ { { 0.0809444479, -0.130504409, 0.116721066 }, { -0.0102485335, 0.0540193266, -0.113614708 }, { -0.000365296938, -0.00412161469, 0.693511405 } } };
            static constexpr M3 kErrorShift{ { { 0.0, 0.0, 0.0 }, { 0.7, 1.0, 0.0 }, { 0.7, 0.0, 1.0 } } };
            M3 simulation{ { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
            switch (mode)
            {
            case ColourVision::RedWeak: simulation = M3{ { { 0.0, 2.02344, -2.52581 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } } }; break;
            case ColourVision::GreenWeak: simulation = M3{ { { 1.0, 0.0, 0.0 }, { 0.494207, 0.0, 1.24827 }, { 0.0, 0.0, 1.0 } } }; break;
            case ColourVision::BlueWeak: simulation = M3{ { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { -0.395913, 0.801109, 0.0 } } }; break;
            case ColourVision::Normal: break;
            }
            const M3 simulated = Mul3(kLmsToRgb, Mul3(simulation, kRgbToLms));
            M3 error{};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    error[i][j] = (i == j ? 1.0 : 0.0) - simulated[i][j];
            const M3 shift = Mul3(kErrorShift, error);
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    m[static_cast<size_t>(j * 5 + i)] = static_cast<float>((i == j ? 1.0 : 0.0) + shift[i][j]);
            return m;
        }
    }

    Ramp IdentityRamp()
    {
        Ramp r{};
        for (int i = 0; i < 256; ++i)
        {
            const uint16_t v = Quantise(i / 255.0);
            r[static_cast<size_t>(i)] = r[static_cast<size_t>(256 + i)] = r[static_cast<size_t>(512 + i)] = v;
        }
        return r;
    }

    bool IsIdentity(const Ramp& ramp)
    {
        const Ramp id = IdentityRamp();
        for (size_t i = 0; i < ramp.size(); ++i)
        {
            if (std::abs(static_cast<int>(ramp[i]) - static_cast<int>(id[i])) > 64)
                return false;
        }
        return true;
    }

    Matrix IdentityMatrix()
    {
        Matrix m{};
        for (int i = 0; i < 5; ++i)
            m[static_cast<size_t>(i * 5 + i)] = 1.0f;
        return m;
    }

    bool IsIdentity(const Matrix& matrix)
    {
        const Matrix id = IdentityMatrix();
        for (size_t i = 0; i < matrix.size(); ++i)
        {
            if (std::abs(matrix[i] - id[i]) > 0.0005f)
                return false;
        }
        return true;
    }

    Matrix Multiply(const Matrix& a, const Matrix& b)
    {
        Matrix r{};
        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < 5; ++j)
            {
                double sum = 0.0;
                for (int k = 0; k < 5; ++k)
                    sum += static_cast<double>(a[static_cast<size_t>(i * 5 + k)]) * b[static_cast<size_t>(k * 5 + j)];
                r[static_cast<size_t>(i * 5 + j)] = static_cast<float>(sum);
            }
        }
        return r;
    }

    Ramp ToneRamp(const ColourLook& look, const GradeCurve& grade)
    {
        const double gamma = std::clamp(look.gamma, 50, 150) / 100.0;
        const double invGamma = 1.0 / gamma;
        const double shadows = Norm(look.shadows), highlights = Norm(look.highlights);
        const double blacks = Norm(look.blacks), whites = Norm(look.whites);
        const double shadowTint = Norm(look.shadowTint), highlightTint = Norm(look.highlightTint);

        const auto curve = [&](double x) {
            double v = std::pow(x, invGamma);
            const double low = Weight(1.0 - v, grade.regionSharpness);
            const double high = Weight(v, grade.regionSharpness);
            v += shadows * low * grade.regionRange;
            v += highlights * high * grade.regionRange;
            const double floor = Weight(1.0 - v, grade.pointSharpness);
            const double ceiling = Weight(v, grade.pointSharpness);
            v += blacks * floor * grade.pointRange;
            v += whites * ceiling * grade.pointRange;
            return std::clamp(v, 0.0, 1.0);
        };
        // Tints lean red (+1) or blue (-1): warm is more red and less blue.
        const auto channel = [&](double x, int lean) {
            double v = curve(x);
            if (lean == 0 || (shadowTint == 0.0 && highlightTint == 0.0))
                return v;
            const double low = Weight(1.0 - v, grade.regionSharpness);
            const double high = Weight(v, grade.regionSharpness);
            v += lean * (shadowTint * low + highlightTint * high) * grade.tintRange;
            return std::clamp(v, 0.0, 1.0);
        };

        Ramp r{};
        for (int i = 0; i < 256; ++i)
        {
            const double x = i / 255.0;
            r[static_cast<size_t>(i)] = Quantise(channel(x, 1));
            r[static_cast<size_t>(256 + i)] = Quantise(channel(x, 0));
            r[static_cast<size_t>(512 + i)] = Quantise(channel(x, -1));
        }
        MakeMonotonic(r);
        return r;
    }

    Ramp ComposeChannels(const Ramp& tone, const ColourLook& look, float warmth, float dim)
    {
        double brightness = std::clamp(look.brightness, 50, 170) / 100.0;
        const double contrast = std::clamp(look.contrast, 50, 150) / 100.0;
        double temperature = std::clamp(look.temperature, -100, 100) / 100.0;
        // Eye care sits on top: at least this warm, and dimmed.
        temperature = std::max(temperature, static_cast<double>(std::clamp(warmth, 0.0f, 1.0f)));
        brightness *= 1.0 - std::clamp(dim, 0.0f, 0.8f);
        const double gains[3] = { std::clamp(look.redGain, 50, 150) / 100.0, std::clamp(look.greenGain, 50, 150) / 100.0,
            std::clamp(look.blueGain, 50, 150) / 100.0 };
        const bool neutral = std::abs(brightness - 1.0) < 0.001 && std::abs(contrast - 1.0) < 0.001 && std::abs(temperature) < 0.001 &&
            gains[0] == 1.0 && gains[1] == 1.0 && gains[2] == 1.0;
        if (neutral)
            return tone;

        // Warm cuts blue hard and green a little; cool cuts red hard.
        const double warm = std::max(0.0, temperature);
        const double cool = std::max(0.0, -temperature);
        const double scale[3] = { brightness * (1.0 - 0.38 * cool) * contrast, brightness * (1.0 - 0.12 * (warm + cool)) * contrast,
            brightness * (1.0 - 0.38 * warm) * contrast };
        const double offset = 0.5 * (1.0 - contrast);

        Ramp r{};
        for (int c = 0; c < 3; ++c)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double x = std::clamp(((i / 255.0) * scale[c] + offset) * gains[c], 0.0, 1.0);
                const int index = std::clamp(static_cast<int>(std::lround(x * 255.0)), 0, 255);
                r[static_cast<size_t>(c * 256 + i)] = tone[static_cast<size_t>(c * 256 + index)];
            }
        }
        MakeMonotonic(r);
        return r;
    }

    Ramp ComposeMonitor(const Ramp& base, int gammaPercent, int warmthPercent)
    {
        gammaPercent = std::clamp(gammaPercent, 50, 150);
        warmthPercent = std::clamp(warmthPercent, 0, 100);
        if (gammaPercent == 100 && warmthPercent == 0)
            return base;
        const double exponent = 100.0 / gammaPercent;
        const double w = warmthPercent / 100.0;
        const double gains[3] = { 1.0, 1.0 - 0.12 * w, 1.0 - 0.38 * w };
        Ramp r{};
        for (int c = 0; c < 3; ++c)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double x = base[static_cast<size_t>(c * 256 + i)] / 65535.0;
                r[static_cast<size_t>(c * 256 + i)] = Quantise(std::pow(x, exponent) * gains[c]);
            }
        }
        return r;
    }

    Ramp DisplayRamp(const ColourLook& look, const EyeCare& eyeCare, int monitorGammaPercent, int monitorWarmthPercent)
    {
        const Ramp tone = ToneRamp(look);
        const Ramp channels = ComposeChannels(tone, look, eyeCare.warmth / 100.0f, eyeCare.dim / 100.0f);
        return ComposeMonitor(channels, monitorGammaPercent, monitorWarmthPercent);
    }

    Ramp Toward(const Ramp& wanted, double strength)
    {
        const Ramp id = IdentityRamp();
        Ramp r{};
        for (size_t i = 0; i < r.size(); ++i)
            r[i] = static_cast<uint16_t>(std::clamp(std::round(id[i] + (static_cast<double>(wanted[i]) - id[i]) * strength), 0.0, 65535.0));
        return r;
    }

    float SoftwareVibranceFactor(int vibrance, bool driverAvailable)
    {
        if (driverAvailable)
            return vibrance > 100 ? vibrance / 100.0f : 1.0f;
        if (vibrance <= 50)
            return vibrance / 50.0f;
        return 1.0f + (vibrance - 50) / 150.0f;
    }

    int DriverLevel(int vibrance)
    {
        return std::clamp(vibrance, 0, 100);
    }

    std::optional<Matrix> ColourMatrix(const ColourLook& look, bool driverAvailable)
    {
        const float saturation = std::clamp(look.saturation, 0, 300) / 100.0f;
        const float vibrance = SoftwareVibranceFactor(std::clamp(look.vibrance, 0, 350), driverAvailable);
        if (std::abs(saturation - 1.0f) < 0.001f && std::abs(vibrance - 1.0f) < 0.001f && look.hue == 0 && look.vision == ColourVision::Normal)
            return std::nullopt;

        // Vibrance holds red back a little so skin tones stay put.
        const float redHold = 1.0f + (vibrance - 1.0f) * 0.55f;
        Matrix m = SaturationMatrix(saturation * redHold, saturation * vibrance, saturation * vibrance);
        if (look.hue != 0)
            m = Multiply(m, HueMatrix(look.hue));
        if (look.vision != ColourVision::Normal)
            m = Multiply(m, VisionMatrix(look.vision));
        if (IsIdentity(m))
            return std::nullopt;
        return m;
    }

    void ApplyToPixels(uint32_t* bgra, size_t count, const Ramp* ramp, const Matrix* matrix)
    {
        // Byte lookup tables from the ramp's high bytes, one per channel.
        std::array<uint8_t, 768> lut{};
        for (size_t i = 0; i < lut.size(); ++i)
            lut[i] = ramp ? static_cast<uint8_t>((*ramp)[i] >> 8) : static_cast<uint8_t>(i & 0xFF);
        const auto toByte = [](float v) {
            return v <= 0.0f ? uint8_t{ 0 } : (v >= 1.0f ? uint8_t{ 255 } : static_cast<uint8_t>(v * 255.0f + 0.5f));
        };
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t p = bgra[i];
            uint8_t b = static_cast<uint8_t>(p & 0xFF);
            uint8_t g = static_cast<uint8_t>((p >> 8) & 0xFF);
            uint8_t r = static_cast<uint8_t>((p >> 16) & 0xFF);
            if (matrix)
            {
                const Matrix& m = *matrix;
                const float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
                r = toByte(rf * m[0] + gf * m[5] + bf * m[10] + m[20]);
                g = toByte(rf * m[1] + gf * m[6] + bf * m[11] + m[21]);
                b = toByte(rf * m[2] + gf * m[7] + bf * m[12] + m[22]);
            }
            r = lut[r];
            g = lut[256 + g];
            b = lut[512 + b];
            bgra[i] = (p & 0xFF000000u) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
        }
    }
}
