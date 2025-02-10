#pragma once

#include <Quotient/util.h>

// #include <QtCore/QMetaType> // For Q_NAMESPACE and Q_DECLARE_METATYPE

namespace Quotient {
class Room;
}

namespace HtmlFilter {
Q_NAMESPACE

enum Option : unsigned char {
    Default = 0x0,
    //! Treat `<body>` contents as Markdown (toMatrixHtml() only)
    ConvertMarkdown = 0x1,
    //! Treat `<body>` contents as a fragment in a bigger HTML payload
    //! (suppresses markup processing inside HTML elements and `<mx-reply>`
    //! conversion - toMatrixHtml() only)
    Fragment = 0x2,
    //! Stop at tags not allowed in Matrix, instead of ignoring them
    //! (from*Html() functions only)
    Validate = 0x4,
    //! Remove <mx-reply> elements previously used for reply fallbacks
    StripMxReply = 0x8
};
Q_ENUM_NS(Option)
Q_DECLARE_FLAGS(Options, Option)

struct Context {
    Quotient::Room* room;
    Quotient::EventId eventId{};
};

/*! \brief Result structure for HTML parsing
 *
 * This is the return type of from*Html() functions, which, unlike
 * toMatrixHtml(), can't assume that HTML it receives is valid since it either
 * comes from the wire or a user input and therefore need a means to report
 * an error when the parser cannot cope (most often because of incorrectly
 * closed tags but also if plain incorrect HTML is passed).
 *
 * \sa fromMatrixHtml(), fromLocalHtml()
 */
struct Result {
    Q_GADGET
    Q_PROPERTY(QString filteredHtml MEMBER filteredHtml CONSTANT)
    Q_PROPERTY(QString::size_type errorPos MEMBER errorPos CONSTANT)
    Q_PROPERTY(QString errorString MEMBER errorString CONSTANT)

public:
    /// HTML that the filter managed to produce (incomplete in case of error)
    QString filteredHtml {};
    /// The position at which the first error was encountered; -1 if no error
    QString::size_type errorPos = -1;
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
 *       to XHTML, as from*Html() functions do. In case of an error, debug
 *       builds will fail on assertion, release builds will silently stop
 *       processing and return what could be processed so far.
 *
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
QString toMatrixHtml(const QString& markup, const Context& context, Options options = Default);

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
 * \param context optional room context
 * \param options whether the algorithm should stop at disallowed HTML tags
 *                 rather than ignore them and try to continue
 * \sa Result
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
Result fromMatrixHtml(const QString& matrixHtml, const Context& context, Options options = Default);

/*! \brief Make the received generic HTML compatible with Qt and convertible
 *         to Matrix
 *
 * This function is similar to fromMatrixHtml() in that it produces HTML that
 * can be fed to Qt components - QTextDocument[Fragment]::fromHtml(),
 * in particular; it also uses the same way to tackle irregularities and errors
 * in HTML and removes tags and attributes that cannot be converted to Matrix.
 * Unlike fromMatrixHtml() that accepts Matrix-flavoured HTML, this function
 * accepts generic HTML and allows a few exceptions compared to the Matrix spec
 * recommendations for HTML; specifically, it preserves the `head` element;
 * and `id`, `class`, and `style` attributes throughout HTML are not restricted,
 * allowing generic CSS stuff to do its job inasmuch as Qt supports that.
 *
 * The case for this function is loading a piece of external HTML into a Qt
 * component in anticipation that this piece will later be translated to Matrix
 * HTML - e.g. drag-n-drop/clipboard paste into the message input control.
 *
 * \sa fromMatrixHtml
 */
Result fromLocalHtml(const QString& html, const Context& context, Options options = Fragment);
}
Q_DECLARE_METATYPE(HtmlFilter::Result)
