// Included after the recording backend in test_renderer.cpp. All entity bytes
// are synthetic; assertions exercise the production room snapshot/filter.
namespace {
struct OwnershipFixture {
    static constexpr unsigned Capacity = 96;
    struct alignas(8) Entity { std::array<std::byte, 0x400> bytes{}; };
    alignas(8) std::array<std::byte, 0x2000> room{};
    alignas(8) std::array<std::byte, 0xa0> vtable{};
    std::array<Entity, Capacity> entities{};
    std::array<std::uintptr_t, Capacity> list{};
    unsigned allocated{}, count{};
    isaac::Snapshot snapshot[isaac::MaxBosses]{};

    OwnershipFixture() {
        assert(regions.empty());
        RegionFor(room.data(), room.size());
        RegionFor(vtable.data(), vtable.size());
        RegionFor(entities.data(), sizeof(entities));
        RegionFor(list.data(), sizeof(list));
        Reset();
    }
    ~OwnershipFixture() { regions.clear(); }
    void Reset() {
        room.fill({}); vtable.fill({}); entities = {}; list.fill(0);
        allocated = count = 0;
        Put(room, isaac::profile::RoomEntities, reinterpret_cast<std::uintptr_t>(list.data()));
        Put(vtable, isaac::profile::EntityIsBossVtableSlot, reinterpret_cast<std::uintptr_t>(&Boss));
        isaac::status.omittedBosses = 0;
    }
    std::uintptr_t Address(unsigned index) const {
        assert(index < allocated);
        return reinterpret_cast<std::uintptr_t>(entities[index].bytes.data());
    }
    template<class T> void Set(unsigned index, std::size_t offset, T value) {
        assert(index < allocated); Put(entities[index].bytes, offset, value);
    }
    unsigned Add(unsigned type, unsigned variant = 0, float hp = 100.0f) {
        assert(allocated < Capacity && count < Capacity);
        const unsigned index = allocated++;
        Set(index, 0, reinterpret_cast<std::uintptr_t>(vtable.data()));
        Set(index, isaac::profile::EntityExists, std::uint8_t{1});
        Set(index, isaac::profile::EntityType, type);
        Set(index, isaac::profile::EntityVariant, variant);
        Set(index, isaac::profile::EntityHp, hp);
        Set(index, isaac::profile::EntityMaxHp, 100.0f);
        Set(index, isaac::EntityInitSeed, 100u + index);
        Set(index, isaac::EntityBirthFrame, 1000u + index);
        list[count++] = Address(index);
        return index;
    }
    void Link(unsigned parent, unsigned child) {
        Set(parent, isaac::EntityChild, Address(child));
        Set(child, isaac::EntityParent, Address(parent));
    }
    unsigned Chain(unsigned type, unsigned variant, unsigned pieces) {
        assert(pieces > 0);
        const unsigned head = Add(type, variant);
        for (unsigned i = 1; i < pieces; ++i) { const auto child = Add(type, variant); Link(child - 1, child); }
        return head;
    }
    void Drop(unsigned index) {
        const auto address = Address(index);
        for (unsigned i = 0; i < count; ++i) if (list[i] == address) {
            for (unsigned j = i; j + 1 < count; ++j) list[j] = list[j + 1];
            --count; return;
        }
        assert(false);
    }
    unsigned Take() {
        Put(room, isaac::profile::RoomEntityCount, count);
        isaac::FeatureFrame frame{reinterpret_cast<std::uintptr_t>(&Boss) - 0x1000, 0,
                                 reinterpret_cast<std::uintptr_t>(room.data()), 480, 270, {nullptr, Text, Rect}};
        bool gideon = false;
        return isaac::SnapshotBosses(frame, snapshot, gideon);
    }
    void Expect(std::initializer_list<unsigned> indices) {
        assert(Take() == indices.size());
        for (const unsigned index : indices) {
            bool found = false;
            for (unsigned i = 0; i < indices.size(); ++i) found |= snapshot[i].address == Address(index);
            assert(found);
        }
    }
};

unsigned TestOwnership() {
    using namespace isaac;
    OwnershipFixture f;
    unsigned cases = 0;
    const auto pass = [&](const char* label) { ++cases; std::printf("Ownership: %s PASS\n", label); };

    f.Chain(62, 0, 7); f.Expect({0});
    pass("one Pin, head and six body pieces");
    f.Chain(62, 0, 7); f.Expect({0, 7});
    pass("two separate Pins with matching type and health");
    std::reverse(f.list.begin(), f.list.begin() + f.count); f.Expect({0, 7});
    pass("heads remain distinct when body pieces precede them in room order");
    f.Set(0, profile::EntityHp, 35.0f); f.Set(1, profile::EntityHp, 90.0f); f.Expect({0, 7});
    for (const auto& value : f.snapshot) if (value.address == f.Address(0)) assert(value.hp == 35.0f && value.maximum == 100.0f);
    pass("bar uses head health rather than summing copied body health");

    for (const auto& rule : data::IgnoreWhenParent) {
        f.Reset(); f.Chain(rule.type, rule.variant, 7); f.Expect({0});
        char name[96]; std::snprintf(name, sizeof(name), "upstream shared-body rule %u.%u", rule.type, rule.variant);
        pass(name);
    }
    f.Reset(); f.Chain(62, 1, 10); f.Expect({0});
    pass("Scolex head and nine body pieces");
    f.Reset(); f.Add(912); f.Add(912); f.Add(912); f.Add(912);
    for (unsigned i = 1; i < 4; ++i) f.Set(i, EntityParent, f.Address(0));
    f.Set(0, EntityChild, f.Address(2)); f.Set(2, EntityChild, f.Address(3)); f.Expect({0});
    pass("Mother parts share one owner without reciprocal child pointers");

    f.Reset(); f.Chain(84, 10, 2); f.Chain(84, 10, 2); f.Expect({0, 2});
    pass("one Satan foot bar per linked pair, two independent pairs preserved");
    f.Reset(); f.Chain(19, 0, 3); f.Chain(19, 1, 3);
    f.Add(79, 0); f.Add(79, 10); f.Link(6, 7);
    f.Add(79, 1); f.Add(79, 11); f.Link(8, 9);
    f.Expect({0,1,2,3,4,5,6,7,8,9});
    pass("Larry Jr, Hollow, Gemini and Steven retain independently damageable bars");
    f.Reset(); f.Chain(62, 0, 7); f.Chain(28, 0, 3); f.Add(20); f.Add(20); f.Expect({0,7,10,11});
    pass("mixed genuine multi-boss encounter remains four bosses");

    f.Reset(); f.Chain(918, 0, 6); f.Expect({0});
    f.Set(2, EntityChild, std::uintptr_t{}); f.Set(3, EntityParent, std::uintptr_t{}); f.Expect({0,3});
    pass("Turdlet split creates two independently eligible heads immediately");
    f.Link(2,3); f.Expect({0});
    pass("Turdlet reattachment suppresses the linked part again");

    f.Reset(); f.Chain(62, 0, 7); f.Set(0, profile::EntityDead, std::uint8_t{1}); f.Expect({});
    pass("dead root does not promote linked Pin body pieces");
    f.Set(0, profile::EntityExists, std::uint8_t{0}); f.Expect({});
    pass("removed root does not promote linked Pin body pieces");
    f.Drop(0); f.Expect({});
    pass("stale parent outside room is not dereferenced or promoted");
    for (unsigned i = 1; i < 7; ++i) {
        f.Set(i, EntityParent, std::uintptr_t{}); f.Set(i, profile::EntityDead, std::uint8_t{1});
    }
    f.Expect({});
    pass("cleared weak links plus native death leave no ghost bars");
    f.Set(1, profile::EntityDead, std::uint8_t{}); f.Set(1, EntityInitSeed, std::uint32_t{9000});
    f.Set(1, EntityBirthFrame, std::uint32_t{9001}); f.Expect({1});
    assert(f.snapshot[0].seed == 9000 && f.snapshot[0].birth == 9001);
    pass("new entity at a former segment address is not hidden by stale state");

    f.Reset(); f.Add(62); f.Add(62); f.Set(1, EntityParent, std::uintptr_t{0xdeadbee8}); f.Expect({0});
    pass("unmapped parent token is never dereferenced for shared-body rules");
    f.Set(1, EntityParent, std::uintptr_t{3}); f.Expect({0});
    pass("malformed parent token cannot crash shared-body filtering");
    f.Set(0, EntityParent, f.Address(1)); f.Set(1, EntityParent, f.Address(0)); f.Expect({});
    pass("cyclic shared-body links cannot loop or invent heads");

    f.Reset(); f.Add(412); f.Add(20); f.Add(20); f.Add(20); f.Link(0,1); f.Link(1,2); f.Expect({0,3});
    pass("Delirium descendants hidden while independent same-type boss stays visible");
    f.Set(1, profile::EntityType, unsigned{5}); f.Set(1, EntityFlags, std::uint64_t{1} << 31); f.Expect({0,3});
    pass("Delirium ancestry crosses a non-displayed intermediate entity");
    f.Set(0, profile::EntityDead, std::uint8_t{1}); f.Expect({3});
    pass("dead Delirium owner still suppresses linked temporary descendants");
    f.Drop(0); f.Expect({2,3});
    pass("readable but out-of-room parent cannot hide current-room bosses");
    f.Set(2, EntityParent, std::uintptr_t{0xdeadbee8}); f.Expect({2,3});
    pass("unmapped ancestor stops safely without hiding independent health");

    f.Reset(); f.Add(20); f.Add(20); f.Link(0,1); f.Link(1,0); f.Expect({0,1});
    pass("ordinary boss ancestry cycle terminates and preserves both bars");
    f.Set(0, EntityParent, f.Address(0)); f.Expect({0,1});
    pass("ordinary self-parent terminates without suppressing independent health");
    f.Reset(); f.Add(20);
    for (unsigned i = 1; i <= MaxParentDepth + 1; ++i) {
        f.Add(i == MaxParentDepth + 1 ? 412 : 5);
        f.Set(i, EntityFlags, std::uint64_t{1} << 31); f.Link(i,i-1);
    }
    f.Expect({0});
    pass("ancestor lookup has a hard 64-link bound");
    f.Set(MaxParentDepth, profile::EntityType, unsigned{412}); f.Expect({});
    pass("Delirium at the last permitted ancestor is still recognized");

    f.Reset(); f.Chain(62, 0, 31);
    for (unsigned i = 0; i < 27; ++i) f.Add(20);
    assert(f.Take() == MaxBosses && status.omittedBosses == 0);
    pass("ignored body pieces do not consume the 28-bar capacity");
    f.Add(20); f.Add(20);
    assert(f.Take() == MaxBosses && status.omittedBosses == 2);
    pass("only excess actual bosses count toward omitted bars");
    status = {};
    return cases;
}
} // namespace
