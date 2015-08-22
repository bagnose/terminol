#include "terminol/support/pattern.hxx"
#include "terminol/support/test.hxx"

namespace {

class Movable : private Uncopyable {
public:
    Movable() noexcept : _formed(true) {}

    Movable(Movable && other) noexcept : _formed(other._formed) {
        other._formed = false;
    }

    Movable & operator= (Movable && other) noexcept {
        _formed = other._formed;
        other._formed = false;
        return *this;
    }

    ~Movable() noexcept = default;

    bool formed() const noexcept { return _formed; }

private:
    bool _formed;
};

void test_move_construct(Test & test) {
    Movable m1;
    test.assert(m1.formed(), "default formed");
    Movable m2;
    test.assert(m2.formed(), "default formed");
    m1 = std::move(m2);
    test.assert(m1.formed(), "formed after move to");
    test.assert(!m2.formed(), "unformed after move from");
}

void test_move_assign(Test & test) {
    Movable m1;
    test.assert(m1.formed(), "default formed");
    Movable m2(std::move(m1));
    test.assert(m2.formed(), "formed after move to");
    test.assert(!m1.formed(), "unformed after move from");
}

} // namespace {anonymous}

int main() {
    Test test("support/pattern");

    test.run("move-construct", &test_move_construct);
    test.run("move-assign", &test_move_assign);

    return 0;
}
