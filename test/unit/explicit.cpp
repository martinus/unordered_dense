#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <unordered_map>
#include <vector>

struct Texture {
    int width;
    int height;
    void* data;
};

struct PerImage {
    std::unordered_map<void*, Texture*> textureIndex;
};

struct Scene {
    std::vector<PerImage> perImage;
    ankerl::unordered_dense::map<void*, Texture*> texturesPerKey;
};

struct AppState {
    Scene scene;
};

TEST_CASE("unit_create_AppState_issue_97") {
    AppState appState{};
}
