#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>

namespace mhdl::vis {


class CHCLSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

    public:
        CHCLSyntaxHighlighter(QTextDocument *parent = 0);

    protected:
        void highlightBlock(const QString &text) override;

    private:
        struct HighlightingRule
        {
            QRegularExpression pattern;
            QTextCharFormat format;
        };
        QVector<HighlightingRule> m_highlightingRules;

        QRegularExpression m_commentStartExpression;
        QRegularExpression m_commentEndExpression;

        QTextCharFormat m_keywordFormat;
        QTextCharFormat m_chclFormat;
        QTextCharFormat m_singleLineCommentFormat;
        QTextCharFormat m_multiLineCommentFormat;
        QTextCharFormat m_quotationFormat;
        QTextCharFormat m_functionFormat;
};

}
