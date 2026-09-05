#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace Orbit {

class Theme {
public:
    // Core Colors
    static inline const QColor SurfaceDeep     {0x12, 0x12, 0x15};
    static inline const QColor SurfaceSidebar  {0x18, 0x18, 0x1c};
    static inline const QColor SurfaceEditor   {0x1e, 0x1e, 0x24};
    static inline const QColor SurfaceHeader   {0x22, 0x22, 0x28};
    static inline const QColor SurfaceCard     {0x26, 0x26, 0x2f};
    static inline const QColor SurfaceHover    {0x2b, 0x2b, 0x35};
    static inline const QColor SurfaceActive   {0x33, 0x33, 0x40};

    static inline const QColor BorderSubtle    {0x2b, 0x2b, 0x36};
    static inline const QColor BorderFocus     {0x4f, 0x8c, 0xf6};

    static inline const QColor TextPrimary     {0xea, 0xea, 0xf0};
    static inline const QColor TextSecondary   {0x96, 0x96, 0xa4};
    static inline const QColor TextMuted       {0x5f, 0x5f, 0x6e};

    static inline const QColor Accent          {0x4f, 0x8c, 0xf6};
    static inline const QColor AccentHover     {0x66, 0x9e, 0xf8};

    // Editor-specific Colors
    static inline const QColor GutterBg        {0x18, 0x18, 0x1d};
    static inline const QColor GutterFg        {0x5c, 0x5c, 0x6d};
    static inline const QColor GutterActiveFg  {0xa4, 0xa4, 0xb8};
    static inline const QColor CurrentLineBg   {0x25, 0x25, 0x2e};
    static inline const QColor SelectionBg     {0x2d, 0x42, 0x68};

    // Font Helpers
    static QFont editorFont(int pointSize = 11);
    static QFont uiFont(int pointSize = 10);
    static QFont monoFont(int pointSize = 10);

    // Global Stylesheet
    static QString applicationStyleSheet();
};

} // namespace Orbit
