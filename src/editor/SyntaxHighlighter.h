#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>
#include <QString>

namespace Orbit {

enum class Language {
    None,
    Cpp,
    JavaScript,
    TypeScript,
    Python,
    Json,
    Xml,
    Css,
    Markdown,
    Yaml,
    Bash,
    CMake
};

struct HighlightingRule {
    QRegularExpression pattern;
    QTextCharFormat format;
};

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);

    void setLanguage(Language lang);
    Language language() const;
    void setLanguageByFileName(const QString &fileName);

    static Language detectLanguage(const QString &fileName);
    static QString languageName(Language lang);

signals:
    void languageChanged(Language lang, const QString &name);

protected:
    void highlightBlock(const QString &text) override;

private:
    void setupFormats();
    void buildRules();

    Language m_language;
    QVector<HighlightingRule> m_rules;

    // Theme formats
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_operatorFormat;
    QTextCharFormat m_tagFormat;
    QTextCharFormat m_attributeFormat;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_inlineCodeFormat;

    // Multi-line block comment expressions
    QRegularExpression m_commentStartExpression;
    QRegularExpression m_commentEndExpression;

    QRegularExpression m_docstringStartExpression;
    QRegularExpression m_docstringEndExpression;

    QRegularExpression m_xmlCommentStartExpression;
    QRegularExpression m_xmlCommentEndExpression;
};

} // namespace Orbit
