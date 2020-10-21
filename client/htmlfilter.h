#pragma once

#include <QtCore/QMetaType> // For Q_NAMESPACE and Q_DECLARE_METATYPE
#include <QtCore/QString>

class QuaternionRoom;

namespace HtmlFilter {
Q_NAMESPACE

enum ParsingMode { Tolerant = 0, Validating = 1 };
Q_ENUM_NS(ParsingMode)

/*! \brief Result structure for Matrix HTML parsing
 *
 * This is the return type of matrixToQt(), which, unlike qtToMatrix(),
 * can't assume that HTML it receives is valid since it either comes from
 * the wire or a user input and therefore need a means to report an error when
 * the parser cannot cope (most often because of incorrectly closed tags).
 *
 * \sa matrixToQt()
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

/*! \brief Make the passed HTML compliant with Matrix requirements
 *
 * This function removes HTML tags disallowed in Matrix; on top of that,
 * it cleans away extra parts (DTD, `head`, top-level `p`, extra `span`
 * inside hyperlinks etc.) added by Qt when exporting QTextDocument
 * to HTML, and converts some formatting accepted in Matrix to tags
 * attributes allowed by the CS API spec.
 *
 * \note This function assumes well-formed XHTML produced by Qt classes; while
 *       it corrects unescaped ampersands (`&`) it doesn't check for other
 *       HTML errors and only closes tags usually not closed in HTML (`br`,
 *       `hr` and `img`). In case of an error, debug builds will fail
 *       on assertion, release builds will silently stop at the first error
 *
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */

QString qtToMatrix(const QString& html, QuaternionRoom* context = nullptr);

/*! \brief Make the received HTML with Matrix attributes compatible with Qt
 *
 * Similar to qtToMatrix(), this function removes HTML tags disallowed in Matrix
 * and cleans away extraneous HTML parts but it does the reverse conversion of
 * Matrix-specific attributes to standard HTML that Qt understands. It can deal
 * with a few more escaping errors compared to qtToMatrix(), but still
 * doesn't recover from missing closing tags except those usually not closed
 * in HTML (`br` etc.). In case of an irrecoverable error the returned structure
 * will contain the error details (position and brief description), along with
 * whatever HTML the function managed to produce before the failure.
 *
 * \sa Result
 * \sa
 * https://matrix.org/docs/spec/client_server/latest#m-room-message-msgtypes
 */
Result matrixToQt(const QString& html, QuaternionRoom* context = nullptr,
                  ParsingMode parsingMode = Tolerant);

}
Q_DECLARE_METATYPE(HtmlFilter::Result)
