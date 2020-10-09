#pragma once

#include <QString>

class QuaternionRoom;

/*! \brief Make the passed HTML compliant with Matrix requirements
 *
 * This function removes HTML tags disallowed in Matrix; on top of that,
 * it cleans away extra parts (DTD, `head`, top-level `p`, extra `span`
 * inside hyperlinks etc.) added by Qt when exporting QTextDocument
 * to HTML, and converts some formatting accepted in Matrix to tags
 * attributes allowed by the CS API spec. It requires at least a `<body>` pair
 * of tags to be there and assumes well-formed HTML (i.e. it doesn't recover
 * from missing closing tags, except `br`, `hr` and `img`).
 *
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
QString filterQtHtmlToMatrix(const QString& html,
                             QuaternionRoom* context = nullptr);

/*! \brief Make the received HTML with Matrix attributes compatible with Qt
 *
 * This function also removes HTML tags disallowed in Matrix (same as
 * filterQtHtmlToMatrix) and cleans away extraneous HTML parts but does the
 * reverse conversion of Matrix-specific attributes to standard HTML that
 * Qt understands. As its counterpart, it requires at least the `body` element
 * to be there and assumes well-formed HTML, not recovering from missing
 * closing tags except those usually not closed in HTML (`br` etc.).
 *
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
QString filterMatrixHtmlToQt(const QString& html,
                             QuaternionRoom* context = nullptr);
