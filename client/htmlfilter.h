#pragma once

#include <QtCore/QMetaType> // For Q_NAMESPACE and Q_DECLARE_METATYPE
#include <QtCore/QString>

class QuaternionRoom;

namespace HtmlFilter {
Q_NAMESPACE

enum Option : unsigned char {
    Default = 0x0,
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    /// Treat `<body>` contents as Markdown (toMatrixHtml() only)
    ConvertMarkdown = 0x1,
#endif
    /// Treat `<body>` contents as a fragment in a bigger HTML payload
    /// (suppresses markup processing inside HTML elements and `<mx-reply>`
    /// conversion - toMatrixHtml() only)
    Fragment = 0x2,
    /// Stop at tags not allowed in Matrix, instead of ignoring them
    /// (from*Html() functions only)
    Validate = 0x4
};
Q_ENUM_NS(Option)
Q_DECLARE_FLAGS(Options, Option)

/*! \brief Result structure for HTML parsing
 *
 * This is the return type of fromMatrixHtml(), which, unlike toMatrixHtml(),
 * can't assume that HTML it receives is valid since it either comes from
 * the wire or a user input and therefore need a means to report an error when
 * the parser cannot cope (most often because of incorrectly
 * closed tags but also if plain incorrect HTML is passed).
 *
 * \sa fromMatrixHtml()
 */
struct Result {
    Q_GADGET
    Q_PROPERTY(QString filteredHtml MEMBER filteredHtml CONSTANT)
    Q_PROPERTY(int errorPos MEMBER errorPos CONSTANT)
    Q_PROPERTY(QString errorString MEMBER errorString CONSTANT)

public:
    /// HTML that the filter managed to produce (incomplete in case of error)
    QString filteredHtml {};
    /// The position at which the first error was encountered; -1 if no error
    int errorPos = -1;
    /// The human-readable error message; empty if no error
    QString errorString {};
};

/*! \brief Convert user input to Matrix-flavoured HTML
 *
 * This function takes user input in \p markup and converts it to the Matrix
 * flavour of HTML. The text in \p markup is treated as-if taken from
 * QTextDocument[Fragment]::toHtml(); however, the body of this HTML is itself
 * treated as (HTML-encoded) markup as well, in assumption that rich text
 * (in QTextDocument sense) is exported as the outer level of HTML while
 * the user adds their own HTML inside that rich text. The function decodes
 * and merges the two levels of markup before converting the resulting HTML
 * to its Matrix flavour.
 *
 * When compiling with Qt 5.14 or newer, it is possible to pass ConvertMarkdown
 * in \p options in order to handle the user's markup as a mix of Markdown and
 * HTML. In that case the function will first turn the Markdown parts to HTML
 * and then merge the resulting HTML snippets with the outer markup.
 *
 * The function removes HTML tags disallowed in Matrix; on top of that,
 * it cleans away extra parts (DTD, `head`, top-level `p`, extra `span`
 * inside hyperlinks etc.) added by Qt when exporting QTextDocument
 * to HTML, and converts some formatting that can be represented in Matrix
 * to tags and attributes allowed by the CS API spec.
 *
 * \note This function assumes well-formed XHTML produced by Qt classes; while
 *       it corrects unescaped ampersands (`&`) it does not try to turn HTML
 *       to XHTML, as fromMatrixHtml() does. In case of an error, debug
 *       builds will fail on assertion, release builds will silently stop
 *       processing and return what could be processed so far.
 *
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
QString toMatrixHtml(const QString& markup, QuaternionRoom* context,
                     Options options = Default);

/*! \brief Make the received HTML with Matrix attributes compatible with Qt
 *
 * Similar to toMatrixHtml(), this function removes HTML tags disallowed in
 * Matrix and cleans away extraneous HTML parts but it does the reverse
 * conversion of Matrix-specific attributes to HTML subset that Qt supports.
 * It can deal with a few more irregularities compared to toMatrixHtml(), but
 * still doesn't recover from, e.g., missing closing tags except those usually
 * not closed in HTML (`br` etc.). In case of an irrecoverable error
 * the returned structure will contain the error details (position and brief
 * description), along with whatever HTML the function managed to produce before
 * the failure.
 *
 * \param matrixHtml text in Matrix HTML that should be converted to Qt HTML
 * \param context optional room context to enrich the text
 * \param options whether the algorithm should stop at disallowed HTML tags
 *                 rather than ignore them and try to continue
 * \sa Result
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
Result fromMatrixHtml(const QString& matrixHtml, QuaternionRoom* context,
                      Options options = Default);
}
Q_DECLARE_METATYPE(HtmlFilter::Result)
