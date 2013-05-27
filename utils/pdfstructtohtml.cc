//========================================================================
//
// pdfstructtohtml.cc
//
// Copyright 2013 Igalia S.L.
//
//========================================================================

#include "config.h"
#include <poppler-config.h>
#include "GlobalParams.h"
#include "parseargs.h"
#include "PDFDocFactory.h"
#include "PDFDoc.h"
#include "StructElement.h"
#include "StructTreeRoot.h"
#include "TextOutputDev.h"
#include "UnicodeMap.h"
#include <assert.h>
#include <string>


static void arrayToRGBColor(Object *value, double &r, double &g, double &b)
{
  Object obj;
  r = value->arrayGet(0, &obj)->getNum();
  g = value->arrayGet(1, &obj)->getNum();
  b = value->arrayGet(2, &obj)->getNum();
}

static const char *tagDecideL(const StructElement *elem)
{
  const Attribute *attr = elem->findAttribute(Attribute::ListNumbering, gTrue);
  Object *value = attr ? attr->getValue() : Attribute::getDefaultValue(Attribute::ListNumbering);

  if (value) {
    if (value->isName("Decimal") ||
        value->isName("UpperRoman") ||
	value->isName("LowerRoman") ||
	value->isName("UpperAlpha") ||
	value->isName("LowerAlphs"))
      return "ol";
  }
  return "ul";
}


static void attrStyleListNumbering(const StructElement*, Attribute::Type, Object *value, GooString *style)
{
  if (value->isName("None"))            style->append("list-style-type: none;");
  else if (value->isName("Disc"))       style->append("list-style-type: disc");
  else if (value->isName("Circle"))     style->append("list-style-type: circle;");
  else if (value->isName("Square"))     style->append("list-style-type: square;");
  else if (value->isName("Decimal"))    style->append("list-style-type: decimal;");
  else if (value->isName("UpperRoman")) style->append("list-style-type: upper-roman;");
  else if (value->isName("LowerRoman")) style->append("list-style-type: lower-roman;");
  else if (value->isName("UpperAlpha")) style->append("list-style-type: upper-latin;");
  else if (value->isName("LowerAlphs")) style->append("list-style-type: lower-latin;");
}

static void attrStyleTextAlign(const StructElement*, Attribute::Type, Object *value, GooString *style)
{
  if (value->isName("Justify"))     style->append("text-align: justify;");
  else if (value->isName("Start"))  style->append("text-align: left;");
  else if (value->isName("End"))    style->append("text-align: right;");
  else if (value->isName("Center")) style->append("text-align: center;");
}

static void attrStyleTextDecoration(const StructElement*, Attribute::Type, Object *value, GooString *style)
{
  if (value->isName("Underline"))        style->append("text-decoration: underline;");
  else if (value->isName("Overline"))    style->append("text-decoration: overline;");
  else if (value->isName("LineThrough")) style->append("text-decoration: line-through;");
  else if (value->isName("None"))        style->append("text-decoration: none;");
}

static void attrColRowSpan(const StructElement*, Attribute::Type type, Object *value, GooString *attrs)
{
  if (value->isNum() && value->getNum() > 0.0) {
    attrs->appendf(" {0:s}span='{1:u}'",
		   (type == Attribute::RowSpan) ? "row" : "col",
		   (unsigned) value->getNum());
  }
}

static void attrStyleColor(const StructElement*, Attribute::Type type, Object *value, GooString *style)
{
  double r, g, b;
  arrayToRGBColor(value, r, g, b);

  if (type == Attribute::BackgroundColor)
    style->append("background-");
  style->appendf("color: rgb({0:u}, {1:u}, {2:u});",
		 ((Guint) (r * 255)) & 0xFF,
		 ((Guint) (g * 255)) & 0xFF,
		 ((Guint) (b * 255)) & 0xFF);
}


typedef void (*TagAttrBuildFunc)(const StructElement*, Attribute::Type, Object*, GooString*);

static const struct AttrMapEntry {
  Attribute::Type  type;
  GBool            style;
  TagAttrBuildFunc buildAttr;
} attrBuildMap[] = {
  { Attribute::Color,              gTrue,  attrStyleColor          },
  { Attribute::BackgroundColor,    gTrue,  attrStyleColor          },
  { Attribute::ListNumbering,      gTrue,  attrStyleListNumbering  },
  { Attribute::TextAlign,          gTrue,  attrStyleTextAlign      },
  { Attribute::TextDecorationType, gTrue,  attrStyleTextDecoration },
  { Attribute::RowSpan,            gFalse, attrColRowSpan          },
  { Attribute::ColSpan,            gFalse, attrColRowSpan          },
};


typedef const char* (*TagDecideFunc)(const StructElement*);

static const struct ElementMapEntry {
  StructElement::Type type;
  const char         *tagName;
  TagDecideFunc       tagDecide;
} elementMap[] = {
  { StructElement::P,     "p",     NULL },
  { StructElement::H1,    "h1",    NULL },
  { StructElement::H2,    "h2",    NULL },
  { StructElement::H3,    "h3",    NULL },
  { StructElement::H4,    "h4",    NULL },
  { StructElement::H5,    "h5",    NULL },
  { StructElement::H6,    "h6",    NULL },
  { StructElement::L,     NULL,    tagDecideL },
  { StructElement::LI,    "li",    NULL },
  { StructElement::Table, "table", NULL },
  { StructElement::TR,    "tr",    NULL },
  { StructElement::TH,    "th",    NULL },
  { StructElement::TD,    "td",    NULL },
  { StructElement::TBody, "tbody", NULL },
  { StructElement::THead, "thead", NULL },
  { StructElement::Link,  "a",     NULL },
};


static void xmlEscape(FILE *out, const char *str)
{
  while (*str) {
    int ch = *str++;
    switch (ch) {
      case '&' : fputs("&amp;",  out); break;
      case '\'': fputs("&apos;", out); break;
      case '"' : fputs("&quot;", out); break;
      case '<' : fputs("&lt;",   out); break;
      case '>' : fputs("&gt;",   out); break;
      default  : fputc(ch, out);
    }
  }
}


class StructVisitor
{
public:
  StructVisitor(PDFDoc *docA, FILE *output):
    inTable(gFalse),
    doc(docA),
    out(output)
  {
    assert(doc);
    assert(out);
  }

  GBool process();

private:
  void start();
  void finish();
  void visit(const StructElement *elem);
  GooString *buildTagAttributes(const StructElement *elem);
  void outputCSSForFont(GfxFont *font);

  void E(const char *str) { xmlEscape(out, str); }
  void O(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
  }

  GBool   inTable;
  PDFDoc *doc;
  FILE   *out;
};


GBool StructVisitor::process()
{
  StructTreeRoot *root = doc->getStructTreeRoot();
  if (!root)
    return gFalse;

  start();
  for (unsigned i = 0; i < root->getNumChildren(); i++)
    visit(root->getChild(i));
  finish();

  return gTrue;
}

void StructVisitor::start()
{
  O("<!DOCTYPE html>\n"
    "<html>\n"
    " <head>\n"
    "  <meta charset='utf-8'>\n");

  Object info;
  if (doc->getDocInfo(&info)->isDict()) {
    Object obj;
    if (info.dictLookup("Title", &obj)->isString()) {
      O("  <title>"); E(obj.getString()->getCString()); O("</title>\n");
    }
  }

  O("  <style type='text/css'>\n"
    "   * { color: #222; }\n"
    "   table, tbody, thead, tr { margin: 0; padding: 0; }\n"
    "   table { border-collapse: collapse; margin: 0 auto; }\n"
    "   th, td { margin: 0; padding: 0.15em 0.5em; border: 1px solid #aaa; }\n"
    "   th { background: #eee; }\n"
    "  </style>\n"
    " </head>\n"
    " <body>\n\n");
}

void StructVisitor::finish()
{
  O("\n"
    " </body>\n"
    "</html>\n");
}


GooString *StructVisitor::buildTagAttributes(const StructElement *elem)
{
  GooString *style = NULL;
  GooString *attrs = NULL;

  if (elem->getType() == StructElement::Link) {
    // XXX This is crude, but gets the job done.
    for (unsigned i = 0; i < elem->getNumChildren(); i++) {
      if (elem->getChild(i)->isObjectRef()) {
	Ref ref = elem->getChild(i)->getObjectRef();
	Object value;
	if (doc->getXRef()->fetch(ref.num, ref.gen, &value)->isDict("Annot")) {
	  Object action;
	  if (value.dictLookup("A", &action)->isDict("Action")) {
	    Object uri;
	    if (action.dictLookup("URI", &uri)->isString()) {
	      if (!attrs) attrs = new GooString();
	      attrs->appendf(" href='{0:s}'", uri.getString());
	    }
	    uri.free();
	  }
	  action.free();
	}
	value.free();
	break;
      }
    }
  }

  for (unsigned i = 0; i < elem->getNumAttributes(); i++) {
    const Attribute *attr = elem->getAttribute(i);
    const AttrMapEntry *entry = NULL;
    for (unsigned j = 0; j < sizeof(attrBuildMap) / sizeof(attrBuildMap[0]); j++) {
      if (attrBuildMap[j].type == attr->getType()) {
	entry = &attrBuildMap[j];
	break;
      }
    }
    if (entry) {
      if (entry->style) {
	if (!style) style = new GooString();
	(*entry->buildAttr)(elem, attr->getType(), attr->getValue(), style);
      } else {
	if (!attrs) attrs = new GooString();
	(*entry->buildAttr)(elem, attr->getType(), attr->getValue(), attrs);
      }
    }
  }

  if (style) {
    if (!attrs) attrs = new GooString();
    attrs->append(" style='");
    attrs->append(style);
    attrs->append("'");
  }

  return attrs;
}


void StructVisitor::outputCSSForFont(GfxFont *font)
{
  if (font->isSerif())
    O("serif; ");
  else if (font->isFixedWidth())
    O("monospace; ");
  else
    O("sans-serif; ");

  switch (font->getWeight()) {
    case GfxFont::W100: O("font-weight: 100; "); break;
    case GfxFont::W200: O("font-weight: 200; "); break;
    case GfxFont::W300: O("font-weight: 300; "); break;
    case GfxFont::W400: O("font-weight: 400; "); break;
    case GfxFont::W500: O("font-weight: 500; "); break;
    case GfxFont::W600: O("font-weight: 600; "); break;
    case GfxFont::W700: O("font-weight: 700; "); break;
    case GfxFont::W800: O("font-weight: 800; "); break;
    case GfxFont::W900: O("font-weight: 900; "); break;
    case GfxFont::WeightNotDefined:
    default:
      break; // Output nothing
  }

  if (font->isItalic())
    O("font-style: italic; ");
}


void StructVisitor::visit(const StructElement *elem)
{
  if (elem->isContent()) {
    const GooString *text;

    if ((text = elem->getAltText()) || (text = elem->getActualText()))
      E(text->getCString());
    else {
      const TextSpanArray spans(elem->getTextSpans());
      for (TextSpanArray::const_iterator i = spans.begin(); i != spans.end(); ++i) {
        O("<span");
        if (i->getFont()) {
          O(" style='");
          outputCSSForFont(i->getFont());
          O("'");
        }
        O(">%s</span>", i->getText());
      }
    }
  } else {
    const ElementMapEntry *entry = NULL;
    for (unsigned i = 0; i < sizeof(elementMap) / sizeof(elementMap[0]); i++)
      if (elementMap[i].type == elem->getType())
	entry = &elementMap[i];

    if (entry) {
      assert(entry->tagName || entry->tagDecide);
      const char *tag = entry->tagName ? entry->tagName : (*entry->tagDecide)(elem);
      if (!(inTable && entry->type == StructElement::P)) {
        GooString *attrs = buildTagAttributes(elem);
        O("<%s", tag);
        if (attrs) O("%s", attrs->getCString());
	delete attrs;
	O(">");
      }
      if (entry->type == StructElement::Table)
        inTable = gTrue;
      for (unsigned i = 0; i < elem->getNumChildren(); i++)
	visit(elem->getChild(i));
      inTable = gFalse;
      if (!(inTable && entry->type == StructElement::P))
        O("</%s>\n", tag);
    } else {
      for (unsigned i = 0; i < elem->getNumChildren(); i++)
	visit(elem->getChild(i));
    }
  }
}


static char ownerPassword[33] = "\001";
static char userPassword[33]  = "\001";
static GBool printHelp        = gFalse;

static const ArgDesc argDesc[] = {
  { "-opw", argString, ownerPassword, sizeof(ownerPassword),
    "owner password (for encrypted files)" },
  { "-upw", argString, userPassword, sizeof(userPassword),
    "user password (for encrypted files)" },
  { "-h", argFlag, &printHelp, 0,
    "print usage information" },
  { "-help", argFlag, &printHelp, 0,
    "print usage information" },
  { "--help", argFlag, &printHelp, 0,
    "print usage information" },
  { NULL }
};

int main(int argc, char **argv)
{
  PDFDoc *doc;
  GooString *ownerPW, *userPW, *fileName;
  GBool ok;
  int exitCode = 99;
  StructVisitor *v = NULL;
  FILE *output = stdout;
  char utf8TextEncoding[] = "UTF-8";

  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || (argc < 2) || (argc > 3) || printHelp) {
    fprintf(stderr, "pdfstructtohtml version " PACKAGE_VERSION "\n");
    fprintf(stderr, "%s\n", popplerCopyright);
    fprintf(stderr, "%s\n", xpdfCopyright);
    printUsage("pdfstructtohtml", "<PDF-file> [<HTML-file>]", argDesc);
    if (printHelp)
      exitCode = EXIT_SUCCESS;
    goto err0;
  }

  ownerPW  = (ownerPassword[0] != '\001') ? new GooString(ownerPassword) : NULL;
  userPW   = (userPassword[0]  != '\001') ? new GooString(userPassword)  : NULL;
  fileName = new GooString(argv[1]);

  if (fileName->cmp("-") == 0)
    fileName->Set("fd://0");

  globalParams = new GlobalParams();
  globalParams->setTextEncoding(utf8TextEncoding);

  doc = PDFDocFactory().createPDFDoc(*fileName, ownerPW, userPW);
  delete ownerPW;
  delete userPW;

  if (!doc->isOk()) {
    exitCode = 1;
    goto err1;
  }

  if (argc == 3) {
    if (!(output = fopen(argv[2], "wb"))) {
      exitCode = 2;
      goto err1;
    }
  }

  v = new StructVisitor(doc, output);
  v->process();

  if (output != stdout)
    fclose(output);

  delete v;

err1:
  delete doc;
  delete fileName;
  delete globalParams;

err0:
  Object::memCheck(stderr);
  gMemReport(stderr);
  return exitCode;
}
