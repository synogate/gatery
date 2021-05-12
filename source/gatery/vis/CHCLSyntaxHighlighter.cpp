/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "CHCLSyntaxHighlighter.h"

namespace hcl::vis {
    
    
CHCLSyntaxHighlighter::CHCLSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    m_keywordFormat.setForeground(Qt::darkBlue);
    m_keywordFormat.setFontWeight(QFont::Bold);
    const QString keywordPatterns[] = {
        QStringLiteral("\\bauto\\b"), QStringLiteral("\\bIF\\b"), QStringLiteral("\\bELSE\\b"),
        QStringLiteral("\\bchar\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bconst\\b"),
        QStringLiteral("\\bdouble\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bexplicit\\b"),
        QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"), QStringLiteral("\\bint\\b"),
        QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bsize_t\\b"),
        QStringLiteral("\\blong\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\boperator\\b"),
        QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bpublic\\b"),
        QStringLiteral("\\bshort\\b"), QStringLiteral("\\bsignals\\b"), QStringLiteral("\\bsigned\\b"),
        QStringLiteral("\\bslots\\b"), QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bstruct\\b"),
        QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\btypename\\b"),
        QStringLiteral("\\bunion\\b"), QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bvirtual\\b"),
        QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bvolatile\\b"), QStringLiteral("\\bbool\\b"),
        QStringLiteral("\\busing\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("#include\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }
    
    m_chclFormat.setFontWeight(QFont::Bold);
    m_chclFormat.setForeground(Qt::darkMagenta);
    const QString CHCLKeywordsPatterns[] = {
        QStringLiteral("\\bBit\\b"), QStringLiteral("\\bBitVector\\b"),
        QStringLiteral("\\bUnsignedInteger\\b"), QStringLiteral("\\bSignedInteger\\b"),
        QStringLiteral("\\bGroupScope\\b"), QStringLiteral("\\bDesignScope\\b"),
        QStringLiteral("\\bRegisterFactory\\b")
    };
    for (const QString &pattern : CHCLKeywordsPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_chclFormat;
        m_highlightingRules.append(rule);
    }

    m_quotationFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);

    m_functionFormat.setFontItalic(true);
    m_functionFormat.setForeground(Qt::blue);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    const QString CHCLFunctionPatterns[] = {
        QStringLiteral("\\bHCL_NAMED(?=\\()"),
        QStringLiteral("\\bmux(?=\\()"),
        QStringLiteral("\\bdriveWith(?=\\()"),
    };
    for (const QString &pattern : CHCLFunctionPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_chclFormat;
        m_highlightingRules.append(rule);
    }
    
    m_singleLineCommentFormat.setForeground(Qt::red);
    rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    rule.format = m_singleLineCommentFormat;
    m_highlightingRules.append(rule);

    m_multiLineCommentFormat.setForeground(Qt::red);

    m_commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
    m_commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));
}

void CHCLSyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : qAsConst(m_highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(m_commentStartExpression);
    
    while (startIndex >= 0) {
        QRegularExpressionMatch match = m_commentEndExpression.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex
                            + match.capturedLength();
        }
        setFormat(startIndex, commentLength, m_multiLineCommentFormat);
        startIndex = text.indexOf(m_commentStartExpression, startIndex + commentLength);
    }
}
    
}
