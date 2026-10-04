// Issue 320: parse an IFC file and mesh every element, timing the two phases.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "web-ifc/geometry/IfcGeometryProcessor.h"
#include "web-ifc/parsing/IfcLoader.h"
#include "web-ifc/schema/IfcSchemaManager.h"

static double now_s() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s file.ifc\n", argv[0]);
        return 1;
    }
    std::ifstream f(argv[1], std::ios::binary | std::ios::ate);
    std::string content(static_cast<size_t>(f.tellg()), '\0');
    f.seekg(0);
    f.read(content.data(), static_cast<std::streamsize>(content.size()));

    webifc::schema::IfcSchemaManager schema;
    double t0 = now_s();
    webifc::parsing::IfcLoader loader(67108864, 2147483648U, 10000, schema);
    loader.LoadFile([&](char* dest, size_t off, size_t size) {
        uint32_t n = static_cast<uint32_t>(std::min(content.size() - off, size));
        std::memcpy(dest, &content[off], n);
        return n;
    });
    double t1 = now_s();

    size_t meshes = 0;
    size_t verts = 0;
    {
        webifc::geometry::IfcGeometryProcessor geo(
            loader, schema, 12, true, 1.0E-01, 3.0E-04, 3.0E-04, 1.0E-10, 1.0E-04, 10, 150);
        for (auto type : schema.GetIfcElementList()) {
            if (type == webifc::schema::IFCOPENINGELEMENT || type == webifc::schema::IFCSPACE ||
                type == webifc::schema::IFCOPENINGSTANDARDCASE) {
                continue;
            }
            for (auto id : loader.GetExpressIDsWithType(type)) {
                auto mesh = geo.GetFlatMesh(id);
                for (auto& g : mesh.geometries) {
                    verts += geo.GetGeometry(g.geometryExpressID).vertexData.size();
                }
                ++meshes;
            }
        }
    }
    double t2 = now_s();
    std::printf("parse_s %.3f geometry_s %.3f total_s %.3f meshes %zu verts %zu\n", t1 - t0, t2 - t1, t2 - t0, meshes, verts);
}
