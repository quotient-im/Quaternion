#include "htmlfilter.h"

#include <lib/util.h>

#include <QtCore/QRegularExpression>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtCore/QStringBuilder>
#include <QtCore/QDebug>

#include <stack>

using namespace std;

namespace HtmlFilter {

enum Direction : unsigned char { QtToMatrix, MatrixToQt };

static const QString permittedTags[] = {
    "font",       "del", "h1",    "h2",     "h3",      "h4",     "h5",   "h6",
    "blockquote", "p",   "a",     "ul",     "ol",      "sup",    "sub",  "li",
    "b",          "i",   "u",     "strong", "em",      "strike", "code", "hr",
    "br",         "div", "table", "thead",  "tbody",   "tr",     "th",   "td",
    "caption",    "pre", "span",  "img",    "mx-reply"
};

struct PassList {
    const char* tag;
    vector<const char*> allowedAttrs;
};

// See filterTag() on special processing of commented out tags/attributes
static const PassList passLists[] =
    { { "a", { "name", "target", /* "href" - only from permittedSchemes */ } }
    , { "img", { "width", "height", "alt", "title", /* "src" - only 'mxc:' */ } }
    , { "ol", { "start" } }
    , { "font", { "color", "data-mx-color", "data-mx-bg-color" } }
    , { "span", { "color", "data-mx-color", "data-mx-bg-color" } }
    //, { "code", { "class" /* must start with 'language-' */ } } // Special case
};

static const char* const permittedSchemes[] { "http:",   "https:",  "ftp:",
                                              "mailto:", "magnet:", "matrix:" };

static const auto& htmlColorAttr = QStringLiteral("color");
static const auto& htmlStyleAttr = QStringLiteral("style");
static const auto& mxColorAttr = QStringLiteral("data-mx-color");
static const auto& mxBgColorAttr = QStringLiteral("data-mx-bg-color");

template <size_t Len>
inline QStringRef cssValue(const QStringRef& css,
                           const char (&propertyNameWithColon)[Len])
{
    return css.startsWith(propertyNameWithColon)
               ? css.mid(Len - 1).trimmed()
               : QStringRef();
}

using rewrite_t = vector<pair<QString, QXmlStreamAttributes>>;

template <Direction Dir>
rewrite_t filterTag(const QStringRef& tag, QXmlStreamAttributes attributes,
                    bool firstElement)
{
    if (tag == "mx-reply") { // Very special case
        // As per the spec, `<mx-reply>` can only come at the very
        // beginning, with no attributes allowed; and should be
        // treated as `<div>`; so deal with it as a special case
        if (!firstElement)
            return {};

        if constexpr (Dir == MatrixToQt)
            return { { "div", {} } };
        // Otherwise, just pass `<mx-reply>` to the wire in a usual way
    }

    rewrite_t rewrite { { tag.toString(), {} } };
    if (tag == "code") { // Special case
        copy_if(attributes.begin(), attributes.end(),
                back_inserter(rewrite.back().second), [](const auto& a) {
                    return a.qualifiedName() == "class"
                           && a.value().startsWith("language-");
                });
        return rewrite;
    }

    if (find(begin(permittedTags), end(permittedTags), tag) == end(permittedTags))
        return {}; // The tag is not allowed

    const auto it =
        find_if(begin(passLists), end(passLists),
                [&tag](const auto& passCard) { return passCard.tag == tag; });
    if (it == end(passLists))
        return rewrite; // Drop all attributes, pass the tag

    const auto& passList = it->allowedAttrs;
    for (auto&& a: attributes) {
        /// Find the first element in the rewrite that would accept color
        /// attributes (`font` and, only in Matrix HTML, `span`),
        /// and add the passed attribute to it
        const auto& addColorAttr = [&rewrite](const QString& attrName,
                                              const QStringRef& attrValue) {
            auto it = find_if(rewrite.begin(), rewrite.end(),
                              [](const rewrite_t::value_type& element) {
                                  return element.first == "font"
                                         || (Dir == QtToMatrix
                                             && element.first == "span");
                              });
            if (it == rewrite.end())
                it = rewrite.insert(rewrite.end(), { "font", {} });
            it->second.append(attrName, attrValue.toString());
        };

        // Attribute conversions between Matrix and Qt subsets

        if constexpr (Dir == MatrixToQt) {
            if (a.qualifiedName() == mxColorAttr) {
                addColorAttr(htmlColorAttr, a.value());
                continue;
            } else if (a.qualifiedName() == mxBgColorAttr) {
                rewrite.front().second.append(htmlStyleAttr,
                                              "background-color:" + a.value());
                continue;
            }
        } else {
            if (a.qualifiedName() == htmlStyleAttr) {
                // 'style' attribute is not allowed in Matrix; convert
                // everything possible to tags and other attributes
                const auto& cssProperties = a.value().split(';');
                for (auto p: cssProperties) {
                    p = p.trimmed();
                    if (p.isEmpty())
                        continue;
                    if (const auto& v = cssValue(p, "color:"); !v.isEmpty()) {
                        addColorAttr(mxColorAttr, v);
                    } else if (const auto& v = cssValue(p, "background-color:");
                               !v.isEmpty())
                        addColorAttr(mxBgColorAttr, v);
                    else if (const auto& v = cssValue(p, "font-weight:");
                             v == "bold" || v == "bolder" || v.toFloat() > 500)
                        rewrite.emplace_back().first = "b";
                    else if (const auto& v = cssValue(p, "font-style:");
                             v == "italic" || v.startsWith("oblique"))
                        rewrite.emplace_back().first = "i";
                    else {
                        const auto& fontFamilies =
                            cssValue(p, "font-family:").split(',');
                        for (auto ff: fontFamilies) {
                            ff = ff.trimmed();
                            if (ff.isEmpty())
                                continue;
                            if (ff[0] == '\'' || ff[0] == '"')
                                ff = ff.mid(1);
                            if (ff.startsWith("monospace")) {
                                rewrite.emplace_back().first = "code";
                                break;
                            }
                        }
                    }
                }
                continue;
            } else if (a.qualifiedName() == htmlColorAttr)
                addColorAttr(mxColorAttr, a.value()); // Add to 'color'
        }

        // Generic filtering for attributes

        if ((tag == "a" && a.qualifiedName() == "href"
             && any_of(begin(permittedSchemes), end(permittedSchemes),
                       [&a](const char* s) { return a.value().startsWith(s); }))
            || (tag == "img" && a.qualifiedName() == "src"
                && a.value().startsWith("mxc:"))
            || find(passList.begin(), passList.end(), a.qualifiedName())
                   != passList.end())
            rewrite.front().second.push_back(move(a));
    } // for (a: attributes)
    // Remove the original <font> or <span> if they end up without attributes
    // since without attributes they are no-op
    if (!rewrite.empty()
        && (rewrite.front().first == "font" || rewrite.front().first == "span")
        && rewrite.front().second.empty())
        rewrite.erase(rewrite.begin());

    return rewrite;
}

void linkifyLastCharacters(QString& textBuffer, QXmlStreamWriter& writer,
                           QString& target)
{
    if (textBuffer.isEmpty())
        return;

    // We want to reuse Quotient::linkifyUrls() but it works on the whole
    // string rather than produces a stream of tokens that QXmlStreamWriter
    // could use. So we make sure that the writer enters the "characters" state
    // by writing a zero-length string (that may or may not work on other Qt
    // versions...) before bootlegging a mix of characteres and `a` elements
    // behind QXmlStreamWriter's back.
    writer.writeCharacters({});
    textBuffer = textBuffer.toHtmlEscaped(); // The reader unescaped it
    Quotient::linkifyUrls(textBuffer);
    target += textBuffer;
    textBuffer.clear();
}

template <Direction Dir>
Result process(QString html, [[maybe_unused]] QuaternionRoom* context,
               ParsingMode parsingMode = Tolerant)
{
    // Since Qt doesn't have an HTML parser (outside of QTextDocument) process()
    // uses QXmlStreamReader instead, and it's quite picky about properly closed
    // tags and escaped ampersands. This helper tries to convert the passed HTML
    // to something more XHTML-like, so that the XML reader doesn't choke on
    // trivial things like unclosed `br` or `img` tags and unescaped ampersands
    // in `href` attributes.
    constexpr auto ReOpt = QRegularExpression::CaseInsensitiveOption;
    html.replace(QRegularExpression("<([bh]r)[^/<>]*>", ReOpt), "<\\1 />");
    html.replace(QRegularExpression("<img([^/<>])*>", ReOpt), "<img\\1 />");
    // Escape ampersands outside of character entities
    // (HTML tolerates it, XML doesn't)
    html.replace(
        QRegularExpression(
            "&(?!(#[0-9]+|#x[0-9a-fA-F]+|[[:alpha:]_][-[:alnum:]_:.]*);)",
            ReOpt),
        "&amp;");

    Result result;
    if constexpr (Dir != QtToMatrix) {
        // Catch early non-compliant tags and non-tags, before they upset
        // the XML parser.
        for (auto pos = html.indexOf('<'); pos != -1;
             pos = html.indexOf('<', pos)) {

            const auto& escapeLt = [&html, &pos] {
                html.replace(pos, 1, "&lt;");
                pos += 4; // Put pos right after &lt;
            };

            if (pos > html.size() - 3) { // No space for a complete tag
                escapeLt();
                continue;
            }
            const auto tagNamePos = pos + 1 + (html[pos + 1] == '/');
            auto uncheckedHtml = html.midRef(tagNamePos);
            const QLatin1String commentOpen("!--");
            const QLatin1String commentClose("-->");
            if (uncheckedHtml.startsWith(commentOpen)) { // Skip comments
                pos = html.indexOf(commentClose, tagNamePos + commentOpen.size())
                      + commentClose.size();
                continue;
            }
            // Look ahead to detect stray < and escape it
            auto gtPos = html.indexOf('>', tagNamePos);
            if (auto nextLtPos = html.indexOf('<', tagNamePos);
                gtPos == -1 || (nextLtPos != -1 && nextLtPos < gtPos)) {
                escapeLt();
                continue;
            }
            // Check if it's a valid (opening or closing) tag allowed in Matrix
            auto it = find_if(begin(permittedTags), end(permittedTags),
                              [&uncheckedHtml](const QString& tag) {
                                  if (!(uncheckedHtml.size() > tag.size()
                                        && uncheckedHtml.startsWith(tag)))
                                      return false;
                                  const auto& charAfter =
                                      uncheckedHtml[tag.size()];
                                  return charAfter.isSpace() || charAfter == '/'
                                         || charAfter == '>';
                              });
            if (it != end(permittedTags)) { // Got a valid tag, skip to >
                pos = gtPos + 1;
                continue;
            }
            // Invalid tag or non-tag - either remove the abusing piece or stop
            // and report
            if (parsingMode == Validating) {
                result.errorPos = pos;
                result.errorString = "Non-tag or disallowed tag: "
                                     % uncheckedHtml.left(gtPos - tagNamePos);
                return result;
            }
            html.remove(pos, html.indexOf('>', tagNamePos) - pos + 1);
        }
        // Wrap in a no-op tag to make the text look like valid XML; Qt's rich
        // text engine produces valid XHTML so QtToMatrix doesn't need this.
        html = "<body>" % html % "</body>";
    }

    // Now for the actual parsing

    QXmlStreamReader reader(html);
    QXmlStreamWriter writer(&result.filteredHtml);
    writer.setAutoFormatting(false);

    /// The entry in the (outer) stack corresponds to each level in the source
    /// document; the (inner) stack in each entry records open elements in the
    /// target document.
    using open_tags_t = stack<QString, vector<QString>>;
    stack<open_tags_t, vector<open_tags_t>> tagsStack;
    /// Accumulates characters and resolved entry references until the next
    /// tag (opening or closing); used to linkify text parts
    QString textBuffer;
    int bodyOffset = 0;
    for (bool firstElement = true, inAnchor = false; !reader.atEnd();) {
        const auto tokenType = reader.readNext();
        if (bodyOffset == -1) // See below in 'case StartElement:'
            bodyOffset = reader.characterOffset();

        switch (tokenType) {
        case QXmlStreamReader::NoToken:
            Q_ASSERT(reader.tokenType() != QXmlStreamReader::NoToken /*false*/);
            continue;
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::ProcessingInstruction:
            continue;
        case QXmlStreamReader::Invalid:
            result.errorPos = reader.characterOffset() - bodyOffset;
            result.errorString = reader.errorString();
            qCritical().noquote() << "Invalid XHTML:" << html;
            qCritical().nospace() << "Error at char " << result.errorPos << ": "
                                  << result.errorString;
            qCritical().noquote()
                << "Buffer at error:" << html.mid(reader.characterOffset());
            writer.writeEndDocument();
            continue;
        case QXmlStreamReader::EndDocument:
            if (!tagsStack.empty())
                qDebug() << "filterHtml(): Not all HTML tags closed";
            continue;
        case QXmlStreamReader::StartElement: {
            linkifyLastCharacters(textBuffer, writer, result.filteredHtml);

            const auto& tagName = reader.qualifiedName();
            if (tagName == "head") { // Qt pushes styles in it, not interesting
                reader.skipCurrentElement();
                continue;
            }
            if (tagName == "html")
                continue; // Just ignore, get to the content

            if (tagName == "body") { // Skip but note the encounter
                bodyOffset = -1; // Reuse the variable until the next loop
                continue;
            }

            tagsStack.emplace();
            if (tagsStack.size() > 100)
                qCritical() << "CS API spec limits HTML tags depth at 100";

            // Skip the first top-level <p> and replace further top-level
            // `<p>...</p>` with `<br/>...` - kinda controversial but
            // there's no cleaner way to get rid of the single top-level <p>
            // generated by Qt without assuming that it's the only <p>
            // spanning the whole body (copy-pasting rich text from other
            // editors can bring several legitimate paragraphs of text,
            // e.g.). This is also a very special case where a converted tag
            // is immediately closed, unlike the one in the source text;
            // which is why it's checked here rather than in filterTag().
            if (tagName == "p" && tagsStack.size() == 1 /* just added */) {
                if (!firstElement)
                    writer.writeEmptyElement("br");
            } else {
                const auto& rewrite =
                    filterTag<Dir>(tagName, reader.attributes(), firstElement);
                for (const auto& [tag, attrs]: rewrite) {
                    tagsStack.top().push(tag);
                    writer.writeStartElement(tag);
                    writer.writeAttributes(attrs);
                    if (tag == "a")
                        inAnchor = true;
                }
            }
            firstElement = false;
            continue;
        }
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::EntityReference:
        {
            // Outside of links, defer writing until the nearest tag (opening
            // or closing) in order to linkify the whole text piece with all
            // entity references resolved.
            if (!inAnchor)
                textBuffer += reader.text();
            else
                writer.writeCurrentToken(reader);
            continue;
        }
        case QXmlStreamReader::EndElement:
            linkifyLastCharacters(textBuffer, writer, result.filteredHtml);

            if (tagsStack.empty()) {
                const auto& tagName = reader.qualifiedName();
                if (tagName != "body" && tagName != "html")
                    qWarning() << "filterHtml(): empty tags stack, skipping"
                               << ('/' + tagName);
                continue;
            }
            // Close as many elements as were opened in case StartElement
            for (auto& t = tagsStack.top(); !t.empty(); t.pop()) {
                writer.writeEndElement();
                if (t.top() == "a")
                    inAnchor = false;
            }
            tagsStack.pop();
            continue;
        }
    }
    // Qt 5.15 (and most likely others) add a line break after <body>
    if (result.filteredHtml.startsWith('\n'))
        result.filteredHtml.remove(0, 1);

    return result;
}

QString qtToMatrix(const QString& html, QuaternionRoom* context)
{
    const auto& result = process<QtToMatrix>(html, context);
    Q_ASSERT(result.errorPos == -1);
    return result.filteredHtml;
}

Result matrixToQt(const QString& html, QuaternionRoom* context,
                  ParsingMode parsingMode)
{
    return process<MatrixToQt>(html, context, parsingMode);
}

} // namespace HtmlFilter
