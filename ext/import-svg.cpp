
#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#include "import-svg.h"

#include <iostream>
#include <skia/core/SkPath.h>
#include <skia/core/SkString.h>
#include <skia/utils/SkParsePath.h>
#include <skia/pathops/SkPathOps.h>
#include <tinyxml2.h>
#include "resolve-shape-geometry.h"

#define ARC_SEGMENTS_PER_PI 2
#define ENDPOINT_SNAP_RANGE_PROPORTION (1/16384.)

namespace msdfgen {

#if defined(_DEBUG) || !NDEBUG
#define REQUIRE(cond) { if (!(cond)) { fprintf(stderr, "SVG Parse Error (%s:%d): " #cond "\n", __FILE__, __LINE__); return false; } }
#else
#define REQUIRE(cond) { if (!(cond)) return false; }
#endif

static void skipExtraChars(const char *&pathDef) {
    while (*pathDef == ',' || *pathDef == ' ' || *pathDef == '\t' || *pathDef == '\r' || *pathDef == '\n')
        ++pathDef;
}

static bool readDouble(double &output, const char *&pathDef) {
    skipExtraChars(pathDef);
    int shift;
    double v;
    if (sscanf(pathDef, "%lf%n", &v, &shift) == 1) {
        pathDef += shift;
        output = v;
        return true;
    }
    return false;
}

static bool readAttributeAsDouble(double &output, tinyxml2::XMLElement *element, const char *name) {
	const char* attr = element->Attribute(name);
	return readDouble(output, attr);
}

bool parseShape(tinyxml2::XMLElement* element, SkPath &output) {
	if (tinyxml2::XMLUtil::StringEqual(element->Name(), "circle")) {
		double cx, cy, r;
		REQUIRE(readAttributeAsDouble(cx, element, "cx"));
		REQUIRE(readAttributeAsDouble(cy, element, "cy"));
		REQUIRE(readAttributeAsDouble(r, element, "r"));
        output.addCircle(cx, cy, r);
        return true;
	}

	if (tinyxml2::XMLUtil::StringEqual(element->Name(), "ellipse")) {
		double cx, cy, rx, ry;
		REQUIRE(readAttributeAsDouble(cx, element, "cx"));
		REQUIRE(readAttributeAsDouble(cy, element, "cy"));
		REQUIRE(readAttributeAsDouble(rx, element, "rx"));
		REQUIRE(readAttributeAsDouble(ry, element, "ry"));

        output.addOval(SkRect::MakeXYWH(cx, cy, rx, ry));
        return true;
	}

	if (tinyxml2::XMLUtil::StringEqual(element->Name(), "rect")) {
		double width, height, x, y;
		REQUIRE(readAttributeAsDouble(width, element, "width"));
		REQUIRE(readAttributeAsDouble(height, element, "height"));
		REQUIRE(readAttributeAsDouble(x, element, "x"));
		REQUIRE(readAttributeAsDouble(y, element, "y"));
		
        output.addRect(SkRect::MakeXYWH(x, y, width, height));
        return true;
	}

    std::string asPath;

	if (tinyxml2::XMLUtil::StringEqual(element->Name(), "polygon")) {
        asPath.append("M "); 
        asPath.append(element->Attribute("points")); 
        asPath.append(" Z");
	} else if (tinyxml2::XMLUtil::StringEqual(element->Name(), "path")) {
        asPath.append(element->Attribute("d"));
    } else {
        std::cout << "don't know how to parse: " << element->Value() <<  ", skipping." << std::endl;
        return true;
    }

    if (!SkParsePath::FromSVGString(asPath.c_str(), &output)) {
        tinyxml2::XMLPrinter printer;
        element->Accept(&printer);
        std::cout << "skia failed for: " << element->Value() << std::endl << printer.CStr() << std::endl;
        return false;
    }

    return true;
}

bool recurseSvg(SkPath& everything, tinyxml2::XMLElement* root) {
    for (tinyxml2::XMLElement* e = root->FirstChildElement(); e != NULL; e = e->NextSiblingElement()) {
        if (tinyxml2::XMLUtil::StringEqual(e->Name(), "g")) {
            if (!recurseSvg(everything, e)) return false;
            continue;
        }

        SkPath skPath;

        if (!parseShape(e, skPath)) {
            std::cout << "failed to convert: " << e->Value() << std::endl;
            return false;
        }

        Op(everything, skPath, kUnion_SkPathOp, &everything);
    }

    return true;
}

bool loadSvgShape(Shape &output, const char *filename, int pathIndex, Vector2 *dimensions) {
    tinyxml2::XMLDocument* doc = new tinyxml2::XMLDocument();
    if (doc->LoadFile(filename))
        return false;
    tinyxml2::XMLElement *root = doc->FirstChildElement("svg");
    if (!root)
        return false;

    SkPath skPath;

    if (!recurseSvg(skPath, root)) return false;
	
    SkString debugOut;

    Simplify(skPath, &skPath);
    AsWinding(skPath, &skPath);
    
    // for reasons i do not understand we need to round-trip to an svg string and back to make circles and ellipses render correctly
    SkParsePath::ToSVGString(skPath, &debugOut);
    SkParsePath::FromSVGString(debugOut.c_str(), &skPath);

    shapeFromSkiaPath(output, skPath);
    output.inverseYAxis = true;

    return true;
}

}
