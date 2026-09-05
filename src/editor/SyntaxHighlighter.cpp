#include "SyntaxHighlighter.h"
#include <QFileInfo>
#include <QFont>
#include <QColor>

namespace Orbit {

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_language(Language::None) {

    setupFormats();
}

void SyntaxHighlighter::setupFormats() {
    // Keywords (e.g. import, export, const, return, if) - One Dark Purple
    m_keywordFormat.setForeground(QColor(0xc6, 0x78, 0xdd));
    m_keywordFormat.setFontWeight(QFont::Bold);

    // Types / Built-ins (e.g. string, number, int, void, Promise) - Cyan/Teal
    m_typeFormat.setForeground(QColor(0x4e, 0xc9, 0xb0));

    // Function names (e.g. setServers, print, express) - Sky Blue
    m_functionFormat.setForeground(QColor(0x61, 0xaf, 0xef));

    // String literals ("...", '...', `...`) - Soft Green
    m_stringFormat.setForeground(QColor(0x98, 0xc3, 0x79));

    // Numeric constants (e.g. 8000, 200, 0x3f) - Warm Orange
    m_numberFormat.setForeground(QColor(0xd1, 0x9a, 0x66));

    // Comments (// ..., /* ... */, # ...) - Slate Gray Italic
    m_commentFormat.setForeground(QColor(0x76, 0x7e, 0x8d));
    m_commentFormat.setFontItalic(true);

    // Preprocessor / Decorators (#include, @decorator) - Gold / Amber
    m_preprocessorFormat.setForeground(QColor(0xe5, 0xc0, 0x7b));

    // Operators & Punctuation
    m_operatorFormat.setForeground(QColor(0xab, 0xb2, 0xbf));

    // XML / HTML tags (<div, </span) - Coral / Red
    m_tagFormat.setForeground(QColor(0xe0, 0x6c, 0x75));
    m_tagFormat.setFontWeight(QFont::Bold);

    // Attributes (class=, id=) - Warm Orange
    m_attributeFormat.setForeground(QColor(0xd1, 0x9a, 0x66));

    // Markdown Headings - Sky Blue Bold
    m_headingFormat.setForeground(QColor(0x61, 0xaf, 0xef));
    m_headingFormat.setFontWeight(QFont::Bold);

    // Markdown Bold
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(QColor(0xe5, 0xc0, 0x7b));

    // Markdown Inline Code
    m_inlineCodeFormat.setForeground(QColor(0xe5, 0xc0, 0x7b));

    // Block comments patterns
    m_commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
    m_commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));

    m_docstringStartExpression = QRegularExpression(QStringLiteral("\"\"\"|'''"));
    m_docstringEndExpression = QRegularExpression(QStringLiteral("\"\"\"|'''"));

    m_xmlCommentStartExpression = QRegularExpression(QStringLiteral("<!--"));
    m_xmlCommentEndExpression = QRegularExpression(QStringLiteral("-->"));
}

void SyntaxHighlighter::setLanguage(Language lang) {
    if (m_language == lang) return;
    m_language = lang;
    buildRules();
    rehighlight();
    emit languageChanged(m_language, languageName(m_language));
}

Language SyntaxHighlighter::language() const {
    return m_language;
}

void SyntaxHighlighter::setLanguageByFileName(const QString &fileName) {
    setLanguage(detectLanguage(fileName));
}

Language SyntaxHighlighter::detectLanguage(const QString &fileName) {
    if (fileName.isEmpty()) return Language::None;

    QFileInfo fi(fileName);
    QString baseName = fi.fileName();
    QString ext = fi.suffix().toLower();

    if (baseName.compare("CMakeLists.txt", Qt::CaseInsensitive) == 0 || ext == "cmake") {
        return Language::CMake;
    }
    if (ext == "ts" || ext == "tsx" || ext == "mts" || ext == "cts") {
        return Language::TypeScript;
    }
    if (ext == "js" || ext == "jsx" || ext == "mjs" || ext == "cjs") {
        return Language::JavaScript;
    }
    if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c" ||
        ext == "h" || ext == "hpp" || ext == "hxx") {
        return Language::Cpp;
    }
    if (ext == "py" || ext == "pyw") {
        return Language::Python;
    }
    if (ext == "json") {
        return Language::Json;
    }
    if (ext == "html" || ext == "htm" || ext == "xml" || ext == "svg" || ext == "qrc") {
        return Language::Xml;
    }
    if (ext == "css" || ext == "scss" || ext == "less") {
        return Language::Css;
    }
    if (ext == "md" || ext == "markdown") {
        return Language::Markdown;
    }
    if (ext == "yaml" || ext == "yml") {
        return Language::Yaml;
    }
    if (ext == "sh" || ext == "bash" || ext == "zsh") {
        return Language::Bash;
    }

    return Language::None;
}

QString SyntaxHighlighter::languageName(Language lang) {
    switch (lang) {
        case Language::TypeScript: return QStringLiteral("TypeScript");
        case Language::JavaScript: return QStringLiteral("JavaScript");
        case Language::Cpp:        return QStringLiteral("C++");
        case Language::Python:     return QStringLiteral("Python");
        case Language::Json:       return QStringLiteral("JSON");
        case Language::Xml:        return QStringLiteral("HTML / XML");
        case Language::Css:        return QStringLiteral("CSS");
        case Language::Markdown:   return QStringLiteral("Markdown");
        case Language::Yaml:       return QStringLiteral("YAML");
        case Language::Bash:       return QStringLiteral("Shell");
        case Language::CMake:      return QStringLiteral("CMake");
        case Language::None:
        default:                   return QStringLiteral("Plain Text");
    }
}

void SyntaxHighlighter::buildRules() {
    m_rules.clear();
    if (m_language == Language::None) return;

    auto addKeywordRules = [this](const QStringList &keywords) {
        for (const QString &kw : keywords) {
            m_rules.append({
                QRegularExpression(QStringLiteral("\\b") + kw + QStringLiteral("\\b")),
                m_keywordFormat
            });
        }
    };

    auto addTypeRules = [this](const QStringList &types) {
        for (const QString &type : types) {
            m_rules.append({
                QRegularExpression(QStringLiteral("\\b") + type + QStringLiteral("\\b")),
                m_typeFormat
            });
        }
    };

    switch (m_language) {
        case Language::Cpp: {
            // C++ Keywords
            addKeywordRules({
                "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
                "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
                "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
                "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
                "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
                "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
                "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
                "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
                "public", "reflexpr", "register", "reinterpret_cast", "requires", "return",
                "short", "signed", "sizeof", "static", "static_assert", "static_cast",
                "struct", "switch", "template", "this", "thread_local", "throw", "true",
                "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
                "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
                "signals", "slots", "emit"
            });

            // Standard types & Qt classes
            addTypeRules({
                "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
                "uint64_t", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t", "QString",
                "QByteArray", "QVector", "QList", "QMap", "QHash", "QSet", "QWidget",
                "QObject", "QMainWindow", "QColor", "QFont", "QPoint", "QRect", "QSize",
                "QIcon", "QPixmap", "QFile", "QDir", "QFileInfo", "QTextStream"
            });

            // Preprocessor directives (#include, #define, etc.)
            m_rules.append({QRegularExpression(QStringLiteral("#[a-zA-Z_]\\w*")), m_preprocessorFormat});

            // Functions: identifier followed by '('
            m_rules.append({QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()")), m_functionFormat});

            // Numbers
            m_rules.append({QRegularExpression(QStringLiteral("\\b(?i:0x[0-9a-f]+|\\d+(?:\\.\\d+)?(?:e[+-]?\\d+)?f?)\\b")), m_numberFormat});

            // String literals
            m_rules.append({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])*'")), m_stringFormat});

            // Single line comment: // (excluding :// in URLs)
            m_rules.append({QRegularExpression(QStringLiteral("(?<!:)//[^\n]*")), m_commentFormat});
            break;
        }

        case Language::TypeScript:
        case Language::JavaScript: {
            // JS & TS Keywords
            addKeywordRules({
                "as", "async", "await", "break", "case", "catch", "class", "const",
                "continue", "debugger", "default", "delete", "do", "else", "enum",
                "export", "extends", "false", "finally", "for", "from", "function",
                "get", "if", "implements", "import", "in", "instanceof", "interface",
                "let", "new", "null", "of", "package", "private", "protected", "public",
                "readonly", "return", "set", "static", "super", "switch", "this",
                "throw", "true", "try", "type", "typeof", "undefined", "var", "void",
                "while", "with", "yield"
            });

            // TS & JS standard types / built-ins
            addTypeRules({
                "any", "boolean", "never", "number", "string", "symbol", "unknown",
                "void", "Promise", "Array", "Record", "Map", "Set", "Object", "Function",
                "Date", "RegExp", "Error", "Boolean", "Number", "String"
            });

            // Decorators (@Component, @Injectable)
            m_rules.append({QRegularExpression(QStringLiteral("@[a-zA-Z_]\\w*")), m_preprocessorFormat});

            // Functions: identifier followed by '('
            m_rules.append({QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()")), m_functionFormat});

            // Numbers
            m_rules.append({QRegularExpression(QStringLiteral("\\b(?i:0x[0-9a-f]+|\\d+(?:\\.\\d+)?)\\b")), m_numberFormat});

            // String literals: double quotes, single quotes, template literals
            m_rules.append({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])*'")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("`(?:\\\\.|[^`\\\\])*`")), m_stringFormat});

            // Single line comment: // (excluding :// in URLs)
            m_rules.append({QRegularExpression(QStringLiteral("(?<!:)//[^\n]*")), m_commentFormat});
            break;
        }

        case Language::Python: {
            // Python keywords
            addKeywordRules({
                "and", "as", "assert", "async", "await", "break", "class", "continue",
                "def", "del", "elif", "else", "except", "finally", "for", "from",
                "global", "if", "import", "in", "is", "lambda", "nonlocal", "not",
                "or", "pass", "raise", "return", "try", "while", "with", "yield",
                "True", "False", "None"
            });

            // Python built-in types & functions
            addTypeRules({
                "self", "cls", "int", "str", "float", "bool", "list", "dict", "set",
                "tuple", "bytes", "object", "type", "print", "len", "range",
                "enumerate", "zip", "isinstance", "issubclass", "super", "open"
            });

            // Decorators (@classmethod, @app.route)
            m_rules.append({QRegularExpression(QStringLiteral("@[a-zA-Z_]\\w*(?:\\.[a-zA-Z_]\\w*)*")), m_preprocessorFormat});

            // Functions: def followed by name
            m_rules.append({QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()")), m_functionFormat});

            // Numbers
            m_rules.append({QRegularExpression(QStringLiteral("\\b(?i:0x[0-9a-f]+|\\d+(?:\\.\\d+)?)\\b")), m_numberFormat});

            // Strings (single and double quote single-line)
            m_rules.append({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])*'")), m_stringFormat});

            // Comments (# ...)
            m_rules.append({QRegularExpression(QStringLiteral("#[^\n]*")), m_commentFormat});
            break;
        }

        case Language::Json: {
            // Keys: "key":
            m_rules.append({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"(?=\\s*:)")), m_tagFormat});

            // String values:
            m_rules.append({QRegularExpression(QStringLiteral("(?<=:\\s*)\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("\\[\\s*\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});

            // Numbers
            m_rules.append({QRegularExpression(QStringLiteral("-?\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b")), m_numberFormat});

            // Booleans and null
            addKeywordRules({"true", "false", "null"});
            break;
        }

        case Language::Xml: {
            // Attribute values
            m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("'[^']*'")), m_stringFormat});

            // Attribute names
            m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z0-9_\\-]+(?=\\s*=)")), m_attributeFormat});

            // Tag elements
            m_rules.append({QRegularExpression(QStringLiteral("</?[a-zA-Z0-9_\\-]+")), m_tagFormat});
            m_rules.append({QRegularExpression(QStringLiteral("/?>")), m_tagFormat});
            break;
        }

        case Language::Css: {
            // Selectors (.class, #id)
            m_rules.append({QRegularExpression(QStringLiteral("[.#][a-zA-Z0-9_\\-]+")), m_tagFormat});

            // Properties (color:, margin:)
            m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z\\-]+(?=\\s*:)")), m_attributeFormat});

            // Units & numbers
            m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?(?:px|em|rem|%|vh|vw|s|ms|deg|pt)?\\b")), m_numberFormat});

            // Strings
            m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), m_stringFormat});

            // Important
            m_rules.append({QRegularExpression(QStringLiteral("!important")), m_keywordFormat});
            break;
        }

        case Language::Markdown: {
            // Headings (# Heading)
            m_rules.append({QRegularExpression(QStringLiteral("^#{1,6}\\s+[^\n]*")), m_headingFormat});

            // Bold
            m_rules.append({QRegularExpression(QStringLiteral("\\*\\*[^*]+\\*\\*")), m_boldFormat});

            // Inline code (`code`)
            m_rules.append({QRegularExpression(QStringLiteral("`[^`]+`")), m_inlineCodeFormat});

            // Links ([text](url))
            m_rules.append({QRegularExpression(QStringLiteral("\\[[^\\]]+\\]\\([^\\)]+\\)")), m_stringFormat});

            // Lists
            m_rules.append({QRegularExpression(QStringLiteral("^\\s*[-*+]\\s+")), m_numberFormat});
            break;
        }

        case Language::Yaml: {
            // Keys
            m_rules.append({QRegularExpression(QStringLiteral("^\\s*[a-zA-Z0-9_\\-]+(?=\\s*:)")), m_tagFormat});

            // Booleans & null
            addKeywordRules({"true", "false", "yes", "no", "null", "~"});

            // Numbers
            m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")), m_numberFormat});

            // Strings
            m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'")), m_stringFormat});

            // Comments
            m_rules.append({QRegularExpression(QStringLiteral("#[^\n]*")), m_commentFormat});
            break;
        }

        case Language::Bash: {
            // Shell keywords
            addKeywordRules({
                "case", "do", "done", "elif", "else", "esac", "exit", "export",
                "fi", "for", "function", "if", "in", "local", "return", "then",
                "until", "while", "echo"
            });

            // Shell variables ($VAR, ${VAR})
            m_rules.append({QRegularExpression(QStringLiteral("\\$[a-zA-Z_][a-zA-Z0-9_]*|\\$\\{[^}]+\\}")), m_preprocessorFormat});

            // Numbers
            m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+\\b")), m_numberFormat});

            // Strings
            m_rules.append({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});
            m_rules.append({QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])*'")), m_stringFormat});

            // Comments
            m_rules.append({QRegularExpression(QStringLiteral("#[^\n]*")), m_commentFormat});
            break;
        }

        case Language::CMake: {
            // CMake Commands (set, add_executable, target_link_libraries, etc.)
            m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z_]\\w*(?=\\s*\\()")), m_functionFormat});

            // CMake Variables (${VAR})
            m_rules.append({QRegularExpression(QStringLiteral("\\$\\{[^}]+\\}")), m_preprocessorFormat});

            // Keywords
            addKeywordRules({"TRUE", "FALSE", "ON", "OFF", "REQUIRED", "PRIVATE", "PUBLIC", "INTERFACE"});

            // Strings
            m_rules.append({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")), m_stringFormat});

            // Comments
            m_rules.append({QRegularExpression(QStringLiteral("#[^\n]*")), m_commentFormat});
            break;
        }

        case Language::None:
            break;
    }
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
    if (m_language == Language::None) return;

    // Apply single-line highlighting rules
    for (const HighlightingRule &rule : m_rules) {
        auto matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Default next block state
    setCurrentBlockState(0);

    // Multi-line block comments (C++, JavaScript, TypeScript, CSS)
    if (m_language == Language::Cpp || m_language == Language::JavaScript ||
        m_language == Language::TypeScript || m_language == Language::Css) {

        int startIndex = 0;
        if (previousBlockState() != 1) {
            auto startMatch = m_commentStartExpression.match(text);
            startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
        }

        while (startIndex >= 0) {
            auto endMatch = m_commentEndExpression.match(text, startIndex);
            int endIndex = endMatch.capturedStart();
            int commentLength = 0;

            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }

            setFormat(startIndex, commentLength, m_commentFormat);

            if (endIndex != -1) {
                auto nextStart = m_commentStartExpression.match(text, startIndex + commentLength);
                startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
            } else {
                break;
            }
        }
    }
    // Python multi-line docstrings (""" or ''')
    else if (m_language == Language::Python) {
        int startIndex = 0;
        if (previousBlockState() != 2) {
            auto startMatch = m_docstringStartExpression.match(text);
            startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
        }

        while (startIndex >= 0) {
            auto endMatch = m_docstringEndExpression.match(text, startIndex + 3);
            int endIndex = endMatch.capturedStart();
            int commentLength = 0;

            if (endIndex == -1) {
                setCurrentBlockState(2);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }

            setFormat(startIndex, commentLength, m_commentFormat);

            if (endIndex != -1) {
                auto nextStart = m_docstringStartExpression.match(text, startIndex + commentLength);
                startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
            } else {
                break;
            }
        }
    }
    // XML / HTML multi-line comments (<!-- ... -->)
    else if (m_language == Language::Xml) {
        int startIndex = 0;
        if (previousBlockState() != 4) {
            auto startMatch = m_xmlCommentStartExpression.match(text);
            startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
        }

        while (startIndex >= 0) {
            auto endMatch = m_xmlCommentEndExpression.match(text, startIndex);
            int endIndex = endMatch.capturedStart();
            int commentLength = 0;

            if (endIndex == -1) {
                setCurrentBlockState(4);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }

            setFormat(startIndex, commentLength, m_commentFormat);

            if (endIndex != -1) {
                auto nextStart = m_xmlCommentStartExpression.match(text, startIndex + commentLength);
                startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
            } else {
                break;
            }
        }
    }
}

} // namespace Orbit
