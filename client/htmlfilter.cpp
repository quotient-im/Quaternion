#include "htmlfilter.h"

#include <util.h>

#include <QtGui/QTextDocument>

#include <QtCore/QRegularExpression>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtCore/QStringBuilder>
#include <QtCore/QDebug>

#include <stack>

using namespace std;

namespace HtmlFilter {

enum Dir : unsigned char { QtToMatrix, MatrixToQt };

class Processor {
public:
    [[nodiscard]] static Result process(const QString& html, Dir direction,
                                        QuaternionRoom* context,
                                        Mode mode = Default)
    {
        QString resultHtml;
        QXmlStreamWriter writer(&resultHtml);
        writer.setAutoFormatting(false);
        Processor p { direction, mode, context, writer };
        p.runOn(html);
        return { resultHtml, p.errorPos, p.errorString };
    }

private:
    Dir direction;
    Mode mode;
    QuaternionRoom* context;
    QXmlStreamWriter& writer;
    int errorPos = -1;
    QString errorString {};

    Processor(Dir direction, Mode mode, QuaternionRoom* context,
              QXmlStreamWriter& writer)
        : direction(direction), mode(mode), context(context), writer(writer)
    {}
    void runOn(const QString& html);

    using rewrite_t = vector<pair<QString, QXmlStreamAttributes>>;

    [[nodiscard]] rewrite_t filterTag(const QStringRef& tag,
                                      QXmlStreamAttributes attributes);
    void filterText(QString& textBuffer);
};

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
static const PassList passLists[] = {
    { "a", { "name", "target", /* "href" - only from permittedSchemes */ } },
    { "img", { "width", "height", "alt", "title", "data-mx-emoticon"
               /* "src" - only 'mxc:' */ } },
    { "ol", { "start" } },
    { "font", { "color", "data-mx-color", "data-mx-bg-color" } },
    { "span", { "color", "data-mx-color", "data-mx-bg-color" } }
    //, { "code", { "class" /* must start with 'language-' */ } } // Special case
};

static const char* const permittedSchemes[] { "http:",   "https:",  "ftp:",
                                              "mailto:", "magnet:", "matrix:" };

static const auto& htmlColorAttr = QStringLiteral("color");
static const auto& htmlStyleAttr = QStringLiteral("style");
static const auto& mxColorAttr = QStringLiteral("data-mx-color");
static const auto& mxBgColorAttr = QStringLiteral("data-mx-bg-color");

/*! \brief Massage the passed HTML to look more like XHTML
 *
 * Since Qt doesn't have an HTML parser (outside of QTextDocument)
 * Processor::runOn() uses QXmlStreamReader instead, and it's quite picky
 * about properly closed tags and escaped ampersands. This helper tries
 * to convert the passed HTML to something more XHTML-like, so that
 * the XML reader doesn't choke on trivial things like unclosed `br` or `img`
 * tags and unescaped ampersands in `href` attributes.
 */
[[nodiscard]] QString preprocess(QString html)
{
    // Escape ampersands outside of character entities
    // (HTML tolerates it, XML doesn't)
    html.replace(QRegularExpression("&(?!(#[0-9]+|#x[0-9a-fA-F]+|[[:alpha:]_][-"
                                    "[:alnum:]_:.]*);)",
                                    QRegularExpression::CaseInsensitiveOption),
                 "&amp;");
    return html;
}

QString qtToMatrix(const QString& qtMarkup, QuaternionRoom* context, Mode mode)
{
    const auto& result =
        Processor::process(preprocess(qtMarkup), QtToMatrix, context, mode);
    Q_ASSERT(result.errorPos == -1);
    return result.filteredHtml;
}

Result matrixToQt(const QString& matrixHtml, QuaternionRoom* context,
                  bool validate)
{
    auto html = preprocess(matrixHtml);

    // Catch early non-compliant tags, non-tags and minimised attributes
    // before they upset the XML parser.
    for (auto pos = html.indexOf('<'); pos != -1; pos = html.indexOf('<', pos)) {
        const auto tagNamePos = pos + 1 + (html[pos + 1] == '/');
        const auto uncheckedHtml = html.midRef(tagNamePos);
        const QLatin1String commentOpen("!--");
        const QLatin1String commentClose("-->");
        if (uncheckedHtml.startsWith(commentOpen)) { // Skip comments
            pos = html.indexOf(commentClose, tagNamePos + commentOpen.size())
                  + commentClose.size();
            continue;
        }
        // Look ahead to detect stray < and escape it
        auto gtPos = html.indexOf('>', tagNamePos);
        decltype(pos) nextLtPos;
        if (gtPos == tagNamePos /* <> or </> */ || gtPos == -1 /* no more > */
            || ((nextLtPos = html.indexOf('<', tagNamePos)) != -1
                && nextLtPos < gtPos) /* there's another < before > */) {
            static const auto to = QStringLiteral("&lt;");
            html.replace(pos, 1, to);
            pos += to.size(); // Put pos after the escaped sequence
            continue;
        }
        // Check if it's a valid (opening or closing) tag allowed in Matrix
        const auto tagIt = find_if(begin(permittedTags), end(permittedTags),
                                [&uncheckedHtml](const QString& tag) {
            if (uncheckedHtml.size() <= tag.size()
                || !uncheckedHtml.startsWith(tag, Qt::CaseInsensitive))
                return false;
            const auto& charAfter = uncheckedHtml[tag.size()];
            return charAfter.isSpace() || charAfter == '/' || charAfter == '>';
        });
        if (tagIt == end(permittedTags)) {
            // Invalid tag or non-tag - either remove the abusing piece or stop
            // and report
            if (validate)
                return { {},
                         pos,
                         "Non-tag or disallowed tag: "
                             % uncheckedHtml.left(gtPos - tagNamePos) };

            html.remove(pos, gtPos - pos + 1);
            continue;
        }
        // Got a valid tag

        // Treat minimised attributes (https://www.w3.org/TR/xhtml1/diffs.html#h-4.5)

        // There's no simple way to replace all occurences within
        // a string segment so just go through the segment and insert
        // `=''` after minimized attributes.
        // This is not the place to _filter_ allowed/disallowed attributes -
        // filtering is left for filterTag()
        static const QRegularExpression MinAttrRE {
            R"(([^[:space:]>/"'=]+)\s*(=\s*([^[:space:]>/"']|"[^"]*"|'[^']*')+)?)"
        };
        pos = tagNamePos + tagIt->size();
        QRegularExpressionMatch m;
        while ((m = MinAttrRE.match(html, pos)).hasMatch()
               && m.capturedEnd(1) < gtPos) {
            pos = m.capturedEnd();
            if (m.captured(2).isEmpty()) {
                static const auto attrValue = QString("=''");
                html.insert(m.capturedEnd(1), attrValue);
                gtPos += attrValue.size() - 1;
                pos += attrValue.size() - 1;
            }
        }
        // Make sure empty elements are properly closed
        static const QRegularExpression EmptyElementRE {
            "^img|[hb]r$", QRegularExpression::CaseInsensitiveOption
        };
        if (html[gtPos - 1] != '/' && EmptyElementRE.match(*tagIt).hasMatch()) {
            html.insert(gtPos, '/');
            ++gtPos;
        }
        pos = gtPos + 1;
        Q_ASSERT(pos > 0);
    }
    // Wrap in a no-op tag to make the text look like valid XML; Qt's rich
    // text engine produces valid XHTML so QtToMatrix doesn't need this.
    html = "<body>" % html % "</body>";

    auto result = Processor::process(html, MatrixToQt, context);
    if (result.errorPos == -1) {
        // Make sure to preserve whitespace sequences
        result.filteredHtml = "<span style=\"whitespace: pre-wrap\">"
                              % result.filteredHtml % "</span>";
    }
    return result;
}

void Processor::runOn(const QString &html)
{
    QXmlStreamReader reader { html };

    /// The entry in the (outer) stack corresponds to each level in the source
    /// document; the (inner) stack in each entry records open elements in the
    /// target document.
    using open_tags_t = stack<QString, vector<QString>>;
    stack<open_tags_t, vector<open_tags_t>> tagsStack;

    /// Accumulates characters and resolved entry references until the next
    /// tag (opening or closing); used to linkify (or process Markdown in)
    /// text parts.
    QString textBuffer;
    int bodyOffset = 0;
    bool firstElement = true, inAnchor = false;
    while (!reader.atEnd()) {
        const auto tokenType = reader.readNext();
        if (bodyOffset == -1) // See below in 'case StartElement:'
            bodyOffset = reader.characterOffset();

        if (!textBuffer.isEmpty() && !reader.isCharacters()
            && !reader.isEntityReference())
            filterText(textBuffer);

        switch (tokenType) {
        case QXmlStreamReader::StartElement: {
            const auto& tagName = reader.qualifiedName();
            if (tagsStack.empty()) {
                // These tags are invalid anywhere deeper, and we don't even
                // care to put them to tagsStack
                if (tagName == "html")
                    break; // Just ignore, get to the content inside
                if (tagName == "head") { // Entirely uninteresting
                    reader.skipCurrentElement();
                    break;
                }
                if (tagName == "body") { // Skip but note the encounter
                    bodyOffset = -1; // Reuse the variable until the next loop
                    break;
                }
            }

            tagsStack.emplace();
            if (tagsStack.size() > 100)
                qCritical() << "CS API spec limits HTML tags depth at 100";

            const auto& attrs = reader.attributes();
            if (direction == QtToMatrix) {
                // Qt hardcodes the link style in a `<span>` under `<a>`.
                // This breaks the looks on the receiving side if the sender
                // uses a different style of links from that of the receiver.
                // Since Qt decorates links when importing HTML anyway, we
                // don't lose anything if we just strip away this span tag.
                if (inAnchor && textBuffer.isEmpty() && tagName == "span"
                    && attrs.size() == 1
                    && attrs.front().qualifiedName() == "style")
                    continue; // inAnchor == true ==> firstElement == false
            }
            // Skip the first top-level <p> and replace further top-level
            // `<p>...</p>` with `<br/>...` - kinda controversial but
            // there's no cleaner way to get rid of the single top-level <p>
            // generated by Qt without assuming that it's the only <p>
            // spanning the whole body (copy-pasting rich text from other
            // editors can bring several legitimate paragraphs of text,
            // e.g.). This is also a very special case where a converted tag
            // is immediately closed, unlike the one in the source text;
            // which is why it's checked here rather than in filterTag().
            if (tagName == "p"
                && tagsStack.size() == 1 /* top-level, just emplaced */) {
                if (firstElement)
                    continue; // Skip unsetting firstElement at the loop end
                writer.writeEmptyElement("br");
            } else if (tagName != "mx-reply"
                       || (firstElement && !mode.testFlag(InnerHtml))) {
                // ^ The spec only allows `<mx-reply>` at the very beginning
                const auto& rewrite = filterTag(tagName, attrs);
                for (const auto& [tag, attrs]: rewrite) {
                    tagsStack.top().push(tag);
                    writer.writeStartElement(tag);
                    writer.writeAttributes(attrs);
                    if (tag == "a")
                        inAnchor = true;
                }
            }
            break;
        }
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::EntityReference: {
            if (firstElement && direction == QtToMatrix) {
                // Remove the line break Qt inserts after <body> because it
                // adds an unnecessary whitespace in the HTML context and
                // an unnecessary line break in the Markdown context.
                if (reader.text().startsWith('\n')) {
                    textBuffer += reader.text().mid(1);
                    continue; // Maintain firstElement
                }
            }
            // Outside of links, defer writing until the next non-character,
            // non-entity reference token in order to pass the whole text
            // piece to filterText() with all entity references resolved.
            if (!inAnchor && !mode.testFlag(InnerHtml))
                textBuffer += reader.text();
            else
                writer.writeCurrentToken(reader);
            break;
        }
        case QXmlStreamReader::EndElement:
            if (tagsStack.empty()) {
                const auto& tagName = reader.qualifiedName();
                if (tagName != "body" && tagName != "html")
                    qWarning() << "filterHtml(): empty tags stack, skipping"
                               << ('/' + tagName);
                break;
            }
            // Close as many elements as were opened in case StartElement
            for (auto& t = tagsStack.top(); !t.empty(); t.pop()) {
                writer.writeEndElement();
                if (t.top() == "a")
                    inAnchor = false;
            }
            tagsStack.pop();
            break;
        case QXmlStreamReader::EndDocument:
            if (!tagsStack.empty())
                qWarning().noquote().nospace()
                    << __FUNCTION__ << ": Not all HTML tags closed";
            break;
        case QXmlStreamReader::NoToken:
            Q_ASSERT(reader.tokenType() != QXmlStreamReader::NoToken /*false*/);
            break;
        case QXmlStreamReader::Invalid:
            errorPos = reader.characterOffset() - bodyOffset;
            errorString = reader.errorString();
            qCritical().noquote() << "Invalid XHTML:" << html;
            qCritical().nospace()
                << "Error at char " << errorPos << ": " << errorString;
            qCritical().noquote()
                << "Buffer at error:" << html.mid(reader.characterOffset());
            break;
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::ProcessingInstruction:
            continue; // All these should not affect firstElement state
        }
        // Unset first element once encountered non-whitespace under `<body>`
        firstElement &= (bodyOffset <= 0 || reader.isWhitespace());
    }
}

template <size_t Len>
inline QStringRef cssValue(const QStringRef& css,
                           const char (&propertyNameWithColon)[Len])
{
    return css.startsWith(propertyNameWithColon)
               ? css.mid(Len - 1).trimmed()
               : QStringRef();
}

Processor::rewrite_t Processor::filterTag(const QStringRef& tag,
                                          QXmlStreamAttributes attributes)
{
    if (direction == MatrixToQt) {
        if (tag == "del" || tag == "strike") { // Qt doesn't support these...
            QXmlStreamAttributes attrs;
            attrs.append("style", "text-decoration:line-through");
            return { { "font", std::move(attrs) } };
        }
        if (tag == "mx-reply")
            return { { "div", {} } }; // The spec says that mx-reply is HTML div
        // If `mx-reply` is encountered on the way to the wire, just pass it
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

    /// Find the first element in the rewrite that would accept color
    /// attributes (`font` and, only in Matrix HTML, `span`),
    /// and add the passed attribute to it
    const auto& addColorAttr = [&rewrite, this](const QString& attrName,
                                                const QStringRef& attrValue) {
        auto it = find_if(rewrite.begin(), rewrite.end(),
                          [this](const rewrite_t::value_type& element) {
                              return element.first == "font"
                                     || (direction == QtToMatrix
                                         && element.first == "span");
                          });
        if (it == rewrite.end())
            it = rewrite.insert(rewrite.end(), { "font", {} });
        it->second.append(attrName, attrValue.toString());
    };

    const auto& passList = it->allowedAttrs;
    for (auto&& a: attributes) {
        // Attribute conversions between Matrix and Qt subsets
        if (direction == MatrixToQt) {
            if (a.qualifiedName() == mxColorAttr) {
                addColorAttr(htmlColorAttr, a.value());
                continue;
            }
            if (a.qualifiedName() == mxBgColorAttr) {
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
                    else if (const auto& v = cssValue(p, "text-decoration:");
                             v.contains("line-through"))
                        rewrite.emplace_back().first = "del";
                    else {
                        const auto& fontFamilies =
                            cssValue(p, "font-family:").split(',');
                        for (auto ff: fontFamilies) {
                            ff = ff.trimmed();
                            if (ff.isEmpty())
                                continue;
                            if (ff[0] == '\'' || ff[0] == '"')
                                ff = ff.mid(1);
                            if (ff.startsWith("monospace", Qt::CaseInsensitive)) {
                                rewrite.emplace_back().first = "code";
                                break;
                            }
                        }
                    }
                }
                continue;
            }
            if (a.qualifiedName() == htmlColorAttr)
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

void Processor::filterText(QString& textBuffer)
{
    if (textBuffer.isEmpty())
        return;

    textBuffer = textBuffer.toHtmlEscaped(); // The reader unescaped it

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (mode.testFlag(ConvertMarkdown)) {
        QTextDocument doc;
        doc.setMarkdown(textBuffer);
        textBuffer = doc.toHtml();
    } else
#endif
    {
        Quotient::linkifyUrls(textBuffer);
        textBuffer = "<body>" % textBuffer % "</body>";
    }
    // Re-process this piece of text as HTML but dump text snippets as they are
    Processor(direction, InnerHtml, context, writer).runOn(textBuffer);

    textBuffer.clear();
}

} // namespace HtmlFilter
