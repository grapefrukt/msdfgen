
#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#include "import-svg.h"

#include <cstdio>
#include <tinyxml2.h>
#include "../core/arithmetics.hpp"
#include <cstdarg>
#include <fstream>
#include <skia/core/SkPath.h>
#include <skia/core/SkString.h>
#include <skia/utils/SkParsePath.h>
#include <skia/pathops/SkPathOps.h>
#include "resolve-shape-geometry.h"
#include <iostream>

#define ARC_SEGMENTS_PER_PI 2
#define ENDPOINT_SNAP_RANGE_PROPORTION (1/16384.)

#define STUPID_FIXED_SIZE_BUFFER 2048

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

void formatAsPath(char* output, const char* format, ...) {
    va_list myargs;
    va_start(myargs, format);
    vsnprintf(output, STUPID_FIXED_SIZE_BUFFER, format, myargs);
    va_end(myargs);
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

    char asPath[STUPID_FIXED_SIZE_BUFFER];

	if (tinyxml2::XMLUtil::StringEqual(element->Name(), "polygon")) {
		const char *points = element->Attribute("points");
        formatAsPath(asPath, "M %s Z", points);
	} else if (tinyxml2::XMLUtil::StringEqual(element->Name(), "path")) {
        formatAsPath(asPath, "%s", element->Attribute("d"));
    }

    if (asPath == NULL) {
        std::cout << "don't know how to parse: " << element->Value() << std::endl;
        return false;
    }

    if (!SkParsePath::FromSVGString(asPath, &output)) {
        std::cout << "skia failed for: " << element->Value() << std::endl;
        return false;
    }

    return true;
}

bool recurseSvg(SkPath& everything, tinyxml2::XMLElement* root) {
    for (tinyxml2::XMLElement* e = root->FirstChildElement(); e != NULL; e = e->NextSiblingElement()) {
        if (tinyxml2::XMLUtil::StringEqual(e->Name(), "g")) {
            recurseSvg(everything, e);
            continue;
        }

        SkPath skPath;

        if (!parseShape(e, skPath)) {
            std::cout << "failed to convert: " << e->Value() << std::endl;
            return false;
        }
        std::cout << "found element: " << e->Value() << std::endl;

        Op(everything, skPath, kUnion_SkPathOp, &everything);
    }
}

bool loadSvgShape(Shape &output, const char *filename, int pathIndex, Vector2 *dimensions) {
    tinyxml2::XMLDocument* doc = new tinyxml2::XMLDocument();
    if (doc->LoadFile(filename))
        return false;
    tinyxml2::XMLElement *root = doc->FirstChildElement("svg");
    if (!root)
        return false;

    SkPath skPathEverything;

    recurseSvg(skPathEverything, root);
	
    SkString debugOut;

    SkParsePath::ToSVGString(skPathEverything, &debugOut);
    std::cout << "svg as parsed and merged: " << std::endl << debugOut.c_str() << std::endl;

    Simplify(skPathEverything, &skPathEverything);
    AsWinding(skPathEverything, &skPathEverything);
    
    SkParsePath::ToSVGString(skPathEverything, &debugOut);
    std::cout << "svg after processing: " << std::endl << debugOut.c_str() << std::endl;
       
    shapeFromSkiaPath(output, skPathEverything);
    output.inverseYAxis = true;

    return true;
}

}
