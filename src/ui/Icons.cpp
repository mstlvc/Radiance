#include "ui/Icons.h"

#include "ui/Theme.h"

#include <array>
#include <cmath>
#include <initializer_list>

namespace radiance::ui
{
    namespace
    {
        // Maps the 20x20 design grid onto a box on screen.
        struct Pen
        {
            ImDrawList* dl;
            ImVec2 origin;
            float unit;
            ImU32 colour;
            float stroke;

            ImVec2 P(float x, float y) const { return ImVec2(origin.x + x * unit, origin.y + y * unit); }

            void Line(float x1, float y1, float x2, float y2) const
            {
                dl->AddLine(P(x1, y1), P(x2, y2), colour, stroke);
                Cap(x1, y1);
                Cap(x2, y2);
            }

            void Cap(float x, float y) const
            {
                dl->AddCircleFilled(P(x, y), stroke * 0.5f, colour, 8);
            }

            void Poly(std::initializer_list<ImVec2> points, bool closed = false) const
            {
                ImVec2 mapped[48];
                int n = 0;
                for (const ImVec2& p : points)
                {
                    if (n < 48)
                        mapped[n++] = P(p.x, p.y);
                }
                dl->AddPolyline(mapped, n, colour, closed ? ImDrawFlags_Closed : ImDrawFlags_None, stroke);
                if (!closed && n > 1)
                {
                    dl->AddCircleFilled(mapped[0], stroke * 0.5f, colour, 8);
                    dl->AddCircleFilled(mapped[n - 1], stroke * 0.5f, colour, 8);
                }
            }

            void Circle(float cx, float cy, float r) const
            {
                dl->AddCircle(P(cx, cy), r * unit, colour, 0, stroke);
            }

            void Dot(float cx, float cy, float r) const
            {
                dl->AddCircleFilled(P(cx, cy), r * unit, colour, 0);
            }

            void Box(float x, float y, float w, float h, float r) const
            {
                dl->AddRect(P(x, y), P(x + w, y + h), colour, r * unit, 0, stroke);
            }

            void Arc(float cx, float cy, float r, float fromDeg, float toDeg) const
            {
                constexpr float rad = 3.14159265f / 180.0f;
                dl->PathArcTo(P(cx, cy), r * unit, fromDeg * rad, toDeg * rad, 0);
                dl->PathStroke(colour, ImDrawFlags_None, stroke);
            }

            void Screen() const
            {
                Box(2.5f, 4.0f, 15.0f, 10.0f, 1.5f);
                Line(7.5f, 17.0f, 12.5f, 17.0f);
                Line(10.0f, 14.0f, 10.0f, 17.0f);
            }
        };

        constexpr std::array<ImVec2, 32> kCog{ {
            { 15.70f, 8.47f }, { 18.00f, 8.73f }, { 18.00f, 11.27f }, { 15.70f, 11.53f }, { 15.11f, 12.95f }, { 16.55f, 14.76f },
            { 14.76f, 16.55f }, { 12.95f, 15.11f }, { 11.53f, 15.70f }, { 11.27f, 18.00f }, { 8.73f, 18.00f }, { 8.47f, 15.70f },
            { 7.05f, 15.11f }, { 5.24f, 16.55f }, { 3.45f, 14.76f }, { 4.89f, 12.95f }, { 4.30f, 11.53f }, { 2.00f, 11.27f },
            { 2.00f, 8.73f }, { 4.30f, 8.47f }, { 4.89f, 7.05f }, { 3.45f, 5.24f }, { 5.24f, 3.45f }, { 7.05f, 4.89f },
            { 8.47f, 4.30f }, { 8.73f, 2.00f }, { 11.27f, 2.00f }, { 11.53f, 4.30f }, { 12.95f, 4.89f }, { 14.76f, 3.45f },
            { 16.55f, 5.24f }, { 15.11f, 7.05f },
        } };
    }

    void DrawIcon(ImDrawList* dl, Icon icon, const ImRect& box, ImU32 colour)
    {
        if (icon == Icon::None)
            return;
        const float unit = std::min(box.GetWidth(), box.GetHeight()) / 20.0f;
        const ImVec2 origin(box.Min.x + (box.GetWidth() - unit * 20.0f) * 0.5f, box.Min.y + (box.GetHeight() - unit * 20.0f) * 0.5f);
        const Pen p{ dl, origin, unit, colour, std::max(1.0f, 1.5f * unit) };

        switch (icon)
        {
        case Icon::Display:
        {
            p.Circle(10, 10, 7);
            dl->PathArcTo(p.P(10, 10), 7 * unit, -1.5707963f, 1.5707963f, 0);
            dl->PathFillConvex(colour);
            break;
        }
        case Icon::Resolution:
            p.Screen();
            p.Poly({ { 5.5f, 9.0f }, { 5.5f, 6.5f }, { 8.0f, 6.5f } });
            p.Poly({ { 14.5f, 9.0f }, { 14.5f, 11.5f }, { 12.0f, 11.5f } });
            break;
        case Icon::Monitor:
            p.Screen();
            p.Circle(10, 9, 2.25f);
            break;
        case Icon::Profiles:
            p.Box(1.5f, 6.25f, 17.0f, 8.25f, 4.125f);
            p.Line(4.6f, 10.4f, 8.4f, 10.4f);
            p.Line(6.5f, 8.5f, 6.5f, 12.3f);
            p.Dot(13.5f, 9.8f, 0.9f);
            p.Dot(15.3f, 11.6f, 0.9f);
            break;
        case Icon::Settings:
        {
            ImVec2 pts[kCog.size()];
            for (size_t i = 0; i < kCog.size(); ++i)
                pts[i] = p.P(kCog[i].x, kCog[i].y);
            dl->AddPolyline(pts, static_cast<int>(kCog.size()), colour, ImDrawFlags_Closed, p.stroke);
            p.Circle(10, 10, 2.4f);
            break;
        }
        case Icon::Close:
            p.Line(5.5f, 5.5f, 14.5f, 14.5f);
            p.Line(14.5f, 5.5f, 5.5f, 14.5f);
            break;
        case Icon::Minimise:
            p.Line(5.0f, 10.0f, 15.0f, 10.0f);
            break;
        case Icon::Compare:
            p.Box(3.0f, 4.0f, 14.0f, 12.0f, 1.5f);
            dl->AddRectFilled(p.P(3.0f, 4.0f), p.P(10.0f, 16.0f), colour, 1.5f * unit, ImDrawFlags_RoundCornersLeft);
            break;
        case Icon::Reset:
            p.Arc(10, 10.5f, 6.0f, -150.0f, 150.0f);
            p.Poly({ { 3.6f, 3.8f }, { 4.4f, 7.6f }, { 8.2f, 6.8f } });
            break;
        case Icon::Save:
            p.Line(10.0f, 3.5f, 10.0f, 12.0f);
            p.Poly({ { 6.5f, 8.5f }, { 10.0f, 12.0f }, { 13.5f, 8.5f } });
            p.Poly({ { 4.0f, 13.0f }, { 4.0f, 16.0f }, { 16.0f, 16.0f }, { 16.0f, 13.0f } });
            break;
        case Icon::Trash:
            p.Line(4.0f, 6.0f, 16.0f, 6.0f);
            p.Poly({ { 8.0f, 6.0f }, { 8.0f, 4.0f }, { 12.0f, 4.0f }, { 12.0f, 6.0f } });
            p.Poly({ { 5.5f, 6.0f }, { 6.5f, 16.5f }, { 13.5f, 16.5f }, { 14.5f, 6.0f } });
            break;
        case Icon::Copy:
            p.Box(7.0f, 7.0f, 9.5f, 9.5f, 1.5f);
            p.Poly({ { 4.0f, 12.5f }, { 4.0f, 4.0f }, { 12.5f, 4.0f } });
            break;
        case Icon::Paste:
            p.Box(4.5f, 4.5f, 11.0f, 12.5f, 1.5f);
            p.Box(7.5f, 3.0f, 5.0f, 3.0f, 1.0f);
            break;
        case Icon::Check:
            p.Poly({ { 4.5f, 10.5f }, { 8.5f, 14.5f }, { 15.5f, 6.0f } });
            break;
        case Icon::ChevronDown:
            p.Poly({ { 5.5f, 8.0f }, { 10.0f, 12.5f }, { 14.5f, 8.0f } });
            break;
        case Icon::Plus:
            p.Line(10.0f, 4.5f, 10.0f, 15.5f);
            p.Line(4.5f, 10.0f, 15.5f, 10.0f);
            break;
        case Icon::Edit:
            p.Poly({ { 4.0f, 16.0f }, { 4.6f, 12.9f }, { 13.0f, 4.5f }, { 15.5f, 7.0f }, { 7.1f, 15.4f } }, true);
            p.Line(11.2f, 6.3f, 13.7f, 8.8f);
            break;
        case Icon::ExternalLink:
            p.Poly({ { 9.0f, 5.0f }, { 4.5f, 5.0f }, { 4.5f, 15.5f }, { 15.0f, 15.5f }, { 15.0f, 11.0f } });
            p.Poly({ { 11.5f, 4.0f }, { 16.0f, 4.0f }, { 16.0f, 8.5f } });
            p.Line(16.0f, 4.0f, 9.5f, 10.5f);
            break;
        case Icon::Info:
            p.Circle(10, 10, 7.5f);
            p.Line(10.0f, 9.0f, 10.0f, 13.8f);
            p.Dot(10.0f, 6.4f, 1.0f);
            break;
        case Icon::Verify:
            p.Poly({ { 10.0f, 2.5f }, { 16.0f, 4.8f }, { 16.0f, 9.5f }, { 15.0f, 13.5f }, { 10.0f, 17.5f }, { 5.0f, 13.5f }, { 4.0f, 9.5f }, { 4.0f, 4.8f } }, true);
            p.Poly({ { 7.2f, 10.0f }, { 9.3f, 12.1f }, { 13.0f, 8.0f } });
            break;
        case Icon::Keycap:
            p.Box(2.5f, 5.0f, 15.0f, 10.0f, 2.0f);
            p.Line(6.0f, 12.0f, 14.0f, 12.0f);
            p.Dot(6.0f, 8.4f, 0.9f);
            p.Dot(10.0f, 8.4f, 0.9f);
            p.Dot(14.0f, 8.4f, 0.9f);
            break;
        case Icon::Refresh:
            // Clockwise from the right, round the bottom, up to the top; the head sits on
            // that end and points the way the arc turns.
            p.Arc(10, 10, 6.0f, -30.0f, 270.0f);
            p.Poly({ { 9.4f, 1.6f }, { 11.8f, 4.0f }, { 9.4f, 6.4f } });
            break;
        case Icon::Game:
            p.Box(1.5f, 6.25f, 17.0f, 8.25f, 4.125f);
            p.Line(4.6f, 10.4f, 8.4f, 10.4f);
            p.Line(6.5f, 8.5f, 6.5f, 12.3f);
            p.Dot(13.5f, 9.8f, 0.9f);
            p.Dot(15.3f, 11.6f, 0.9f);
            break;
        case Icon::Terminal:
            p.Box(2.5f, 3.5f, 15.0f, 13.0f, 2.0f);
            p.Poly({ { 6.0f, 7.5f }, { 8.5f, 10.0f }, { 6.0f, 12.5f } });
            p.Line(10.5f, 12.5f, 14.0f, 12.5f);
            break;
        case Icon::None:
            break;
        }
    }

    void DrawMark(ImDrawList* dl, ImVec2 centre, float size, ImU32 colour, ImU32 glow)
    {
        // The Radiance mark: a lit centre with eight rays, each ending in a bead.
        const float u = size / 20.0f;
        const ImVec2 c = centre;
        if (glow)
            dl->AddCircleFilled(c, 7.5f * u, glow, 0);
        dl->AddCircleFilled(c, 3.1f * u, colour, 0);
        constexpr float kStep = 3.14159265f / 4.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float a = kStep * static_cast<float>(i) - 3.14159265f / 2.0f;
            const ImVec2 dir(std::cos(a), std::sin(a));
            const ImVec2 from(c.x + dir.x * 5.6f * u, c.y + dir.y * 5.6f * u);
            const ImVec2 to(c.x + dir.x * 8.2f * u, c.y + dir.y * 8.2f * u);
            dl->AddLine(from, to, colour, std::max(1.0f, 1.7f * u));
            dl->AddCircleFilled(ImVec2(c.x + dir.x * 8.9f * u, c.y + dir.y * 8.9f * u), 1.3f * u, colour, 0);
        }
    }
}
