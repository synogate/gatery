/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>

namespace hcl::vis {


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
