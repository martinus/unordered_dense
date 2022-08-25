#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <unordered_map>
#include <vector>

struct texture {
    int m_width;
    int m_height;
    void* m_data;
};

struct per_image {
    std::unordered_map<void*, texture*> m_texture_index;
};

struct scene {
    std::vector<per_image> m_per_image;
    ankerl::unordered_dense::map<void*, texture*> m_textures_per_key;
};

struct app_state {
    scene m_scene;
};

TEST_CASE("unit_create_AppState_issue_97") {
    app_state app_state{};
}
