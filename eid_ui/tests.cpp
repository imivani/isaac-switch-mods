#include "layout.hpp"
#include "../eid/catalogue.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace isaac_eid_ui;
static Builder builder;
static Layout layout;

static void CheckBounds(const Layout& result, int width) {
    assert(!result.truncated);
    unsigned cells = 0;
    for (unsigned i = 0; i < result.line_count; ++i) {
        const auto& line = result.lines[i];
        assert(line.first == cells && line.count > 0 && line.width <= width);
        for (unsigned j = line.first; j < line.first + line.count; ++j) {
            const auto& cell = result.cells[j];
            assert(cell.y == static_cast<int>(i * LineHeight));
            assert(cell.x >= -2 && cell.x + cell.width <= width);
            if (cell.kind == Kind::Glyph) {
                assert(cell.value != '{' && cell.value != '}');
                assert(Glyph(cell.value));
            }
        }
        cells += line.count;
    }
    assert(cells == result.cell_count);
}

static void ExportLayout(const Layout& result) {
    std::printf("{\"line_count\":%u,\"omitted\":%u,\"cells\":[", result.line_count, result.omitted_variable_paragraphs);
    for (unsigned i = 0; i < result.cell_count; ++i) {
        const auto& cell = result.cells[i];
        std::printf("%s[%u,%u,%u,%d,%d,%d]", i ? "," : "", static_cast<unsigned>(cell.kind), cell.value,
                    cell.color, cell.x, cell.y, cell.width);
    }
    std::puts("]}");
}

int main(int argc, char** argv) {
    const auto catalogue = isaac_eid::EnglishRepentance();
    if (argc == 4 && std::strcmp(argv[1], "--preview") == 0) {
        unsigned category = 0, id = 0;
        std::sscanf(argv[2], "%u", &category);
        std::sscanf(argv[3], "%u", &id);
        isaac_eid::Description description;
        if (!catalogue.Find(static_cast<isaac_eid::Category>(category), id, description)) return 2;
        builder.Build(description.name, 122, false, layout, NamePurple); ExportLayout(layout);
        builder.Build(description.markup, 142, true, layout); ExportLayout(layout);
        return 0;
    }
    unsigned usable = 0, omitted = 0, maximum_lines = 0, maximum_cells = 0;
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const auto& entry = catalogue.entries[i];
        isaac_eid::Description description;
        if (!catalogue.Find(static_cast<isaac_eid::Category>(entry.category), entry.lookup_id, description)) continue;
        ++usable;
        builder.Build(description.markup, 142, true, layout);
        CheckBounds(layout, 142);
        assert(layout.unknown_markup == 0);
        omitted += layout.omitted_variable_paragraphs;
        if (layout.line_count > maximum_lines) maximum_lines = layout.line_count;
        if (layout.cell_count > maximum_cells) maximum_cells = layout.cell_count;
        builder.Build(description.name, 122, false, layout, NamePurple);
        CheckBounds(layout, 122);
        assert(layout.line_count > 0 && layout.line_count <= 3);
    }
    assert(usable == 1108);
    builder.Build("{{Damage}} +1 damage#{{ColorBlue}}Blue{{CR}} white", 142, true, layout);
    CheckBounds(layout, 142);
    assert(layout.cells[0].kind == Kind::Icon);
    assert(std::strcmp(assets::Icons[layout.cells[0].value].name, "Damage") == 0);
    bool blue = false, white_after_blue = false;
    for (unsigned i = 0; i < layout.cell_count; ++i) {
        if (layout.cells[i].color == 0x0000ff) blue = true;
        else if (blue && layout.cells[i].kind == Kind::Glyph && layout.cells[i].color == White) white_after_blue = true;
    }
    assert(blue && white_after_blue);
    builder.Build("Known effect#Gain {CURRENT} damage#Other effect", 142, true, layout);
    assert(layout.omitted_variable_paragraphs == 1 && layout.line_count == 2);
    builder.Build("{{Unrecognized}} Effect", 142, true, layout);
    assert(layout.unknown_markup == 1);
    CheckBounds(layout, 142);
    builder.Build("{{Broken braces", 142, true, layout);
    assert(layout.line_count == 0 && layout.omitted_variable_paragraphs == 1);
    builder.Build("supercalifragilisticexpialidocious", 30, true, layout);
    CheckBounds(layout, 30);
    assert(layout.line_count > 5);
    builder.Build("{{Collectible118}} Brimstone {{Trinket21}}", 142, true, layout);
    CheckBounds(layout, 142);
    bool collectible = false, trinket = false;
    for (unsigned i = 0; i < layout.cell_count; ++i) {
        collectible |= layout.cells[i].kind == Kind::Collectible && layout.cells[i].value == 118;
        trinket |= layout.cells[i].kind == Kind::Trinket && layout.cells[i].value == 21;
    }
    assert(collectible && trinket);
    const char utf8[] = "\xe2\x86\x91 Good \xf0\x9f\x98\x80";
    builder.Build(utf8, 142, true, layout);
    CheckBounds(layout, 142);
    assert(layout.cells[0].kind == Kind::Icon);
    const char invalid[] = "\xe0\x80\x80" "bad";
    builder.Build(invalid, 142, true, layout);
    CheckBounds(layout, 142);
    std::printf("PASS: %u descriptions, exact glyph widths, all markup classified, wrapping, colors, UTF-8 and bounds; "
                "%u variable paragraphs omitted; max %u lines / %u cells\n", usable, omitted, maximum_lines, maximum_cells);
}
