#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include "mandelbrot.hpp"
#include "mandelbrot_fractal_utils.hpp"
#include "mandelbrot_renderer.hpp"
#include "mandelbrot_sender.hpp"
#include "sfml_events_handler.hpp"
#include "sfml_renderer.hpp"
#include "types.hpp"

#include <SFML/Graphics.hpp>
#include <chrono>
#include <optional>
#include <thread>

using namespace std::chrono_literals;

namespace testutil {

template <class... Ts>
struct ValueHolder {
    std::tuple<std::optional<Ts>...> values;
    std::optional<std::exception_ptr> err;
    bool stopped = false;
};

template <class Holder>
struct Receiver {
    using receiver_concept = stdexec::receiver_t;
    Holder *holder;

    template <class... Vs>
    friend void tag_invoke(stdexec::set_value_t, Receiver &&self, Vs &&...vs) noexcept {
        self.store(std::forward<Vs>(vs)...);
    }
    friend void tag_invoke(stdexec::set_error_t, Receiver &&self, std::exception_ptr e) noexcept {
        self.holder->err = e;
    }
    friend void tag_invoke(stdexec::set_stopped_t, Receiver &&self) noexcept { self.holder->stopped = true; }

private:
    template <class... Vs, std::size_t... Is>
    void store_impl(std::index_sequence<Is...>, Vs &&...vs) noexcept {
        (void)std::initializer_list<int>{((std::get<Is>(holder->values) = std::forward<Vs>(vs)), 0)...};
    }
    template <class... Vs>
    void store(Vs &&...vs) noexcept {
        store_impl(std::make_index_sequence<sizeof...(Vs)>{}, std::forward<Vs>(vs)...);
    }
};

}  // namespace testutil

static RenderSettings SmallSettings(unsigned w = 64, unsigned h = 48, unsigned it = 64, double R = 2.0) {
    RenderSettings rs;
    rs.width = w;
    rs.height = h;
    rs.max_iterations = it;
    rs.escape_radius = R;
    return rs;
}

class FrameClock {
public:
    FrameClock() { Reset(); }
    void Reset() noexcept { frame_start_ = std::chrono::steady_clock::now(); }
    auto GetFrameTime() const noexcept { return std::chrono::steady_clock::now() - frame_start_; }

private:
    std::chrono::time_point<std::chrono::steady_clock> frame_start_;
};

class WaitForFPS {
public:
    WaitForFPS(FrameClock &clock, int fps) : clock_(clock), target_fps_(fps) {}
    void operator()() {
        auto elapsed = clock_.GetFrameTime();
        auto target_duration = std::chrono::milliseconds(1000 / target_fps_);
        if (elapsed < target_duration) {
            std::this_thread::sleep_for(target_duration - elapsed);
        }
        clock_.Reset();
    }

private:
    FrameClock &clock_;
    int target_fps_;
};

// --------------------- Utils tests ---------------------
TEST(Utils, CalculateIterationsKnownPoints) {
    using mandelbrot::Complex;
    auto it_in = mandelbrot::CalculateIterationsForPoint(Complex{0.0, 0.0}, 100, 2.0);
    EXPECT_EQ(it_in, 100u);
    auto it_out = mandelbrot::CalculateIterationsForPoint(Complex{2.0, 2.0}, 100, 2.0);
    EXPECT_LT(it_out, 10u);
}

TEST(Utils, PixelToComplexMappingCenterAroundZero) {
    mandelbrot::ViewPort vp;
    auto rs = SmallSettings(60, 40);
    double x_d = static_cast<double>(rs.width) * (0.0 - vp.x_min) / vp.width();
    double y_d = static_cast<double>(rs.height) * (0.0 - vp.y_min) / vp.height();
    auto x = static_cast<std::uint32_t>(x_d);
    auto y = static_cast<std::uint32_t>(y_d);
    auto c = mandelbrot::Pixel2DToComplex(x, y, vp, rs.width, rs.height);
    EXPECT_NEAR(c.real(), 0.0, vp.width() / static_cast<double>(rs.width));
    EXPECT_NEAR(c.imag(), 0.0, vp.height() / static_cast<double>(rs.height));
}

// --------------------- MandelbrotSender tests ---------------------
TEST(MandelbrotSender, ComputesRegionMatrix) {
    auto rs = SmallSettings(32, 24, 50);
    mandelbrot::ViewPort vp;
    PixelRegion region{.start_row = 5, .end_row = 10, .start_col = 0, .end_col = 32};
    PixelMatrix mat = ComputePixelMatrixForRegion(vp, rs, region);
    ASSERT_EQ(mat.size(), region.end_row - region.start_row);
    for (auto &row : mat)
        ASSERT_EQ(row.size(), rs.width);
    auto c = mandelbrot::Pixel2DToComplex(0, region.start_row, vp, rs.width, rs.height);
    auto it = mandelbrot::CalculateIterationsForPoint(c, rs.max_iterations, rs.escape_radius);
    EXPECT_EQ(mat[0][0], it);
}

// --------------------- MandelbrotRenderer::RenderAsync tests ---------------------
TEST(MandelbrotRenderer, RenderAsyncCombinesStripsAndColors) {
    MandelbrotRenderer renderer(4);
    auto rs = SmallSettings(64, 48, 64);
    mandelbrot::ViewPort vp;

    auto sender = renderer.RenderAsync<4>(vp, rs);
    auto tup = stdexec::sync_wait(std::move(sender));
    ASSERT_TRUE(tup.has_value());
    auto result = std::get<0>(*tup);

    ASSERT_EQ(result.pixel_data.size(), rs.height);
    ASSERT_EQ(result.pixel_data[0].size(), rs.width);
    ASSERT_EQ(result.color_data.size(), rs.height);
    ASSERT_EQ(result.color_data[0].size(), rs.width);

    std::uint32_t cx = static_cast<std::uint32_t>(static_cast<double>(rs.width) * (0.0 - vp.x_min) / vp.width());
    std::uint32_t cy = static_cast<std::uint32_t>(static_cast<double>(rs.height) * (0.0 - vp.y_min) / vp.height());
    ASSERT_LT(cx, rs.width);
    ASSERT_LT(cy, rs.height);
    auto it = result.pixel_data[cy][cx];
    auto col = result.color_data[cy][cx];
    if (it == rs.max_iterations) {
        EXPECT_EQ(col.r, 0u);
        EXPECT_EQ(col.g, 0u);
        EXPECT_EQ(col.b, 0u);
    }
}

// --------------------- CalculateMandelbrotAsyncSender tests ---------------------
TEST(CalculateAsync, RespectsNeedRerenderFlag) {
    MandelbrotRenderer renderer(4);
    auto rs = SmallSettings(40, 30, 40);
    AppState state;

    state.need_rerender = false;
    {
        testutil::ValueHolder<RenderResult> holder{};
        auto sender = CalculateMandelbrotAsyncSender(state, rs, renderer);
        auto op = stdexec::connect(sender, testutil::Receiver<decltype(holder)>{&holder});
        stdexec::start(op);
        ASSERT_TRUE(std::get<0>(holder.values).has_value());
        auto rr = *std::get<0>(holder.values);
        EXPECT_TRUE(rr.color_data.empty());
    }

    state.need_rerender = true;
    {
        testutil::ValueHolder<RenderResult> holder{};
        auto sender = CalculateMandelbrotAsyncSender(state, rs, renderer);
        auto op = stdexec::connect(sender, testutil::Receiver<decltype(holder)>{&holder});
        stdexec::start(op);
        ASSERT_TRUE(std::get<0>(holder.values).has_value());
        auto rr = *std::get<0>(holder.values);
        EXPECT_FALSE(rr.color_data.empty());
        EXPECT_FALSE(state.need_rerender);
    }
}

// --------------------- SFMLRender tests ---------------------
TEST(SFMLRenderTest, DrawsPixelsToTexture) {
    auto rs = SmallSettings(16, 12, 32);
    sf::RenderWindow *wnd = nullptr;
    try {
        wnd = new sf::RenderWindow(sf::VideoMode{rs.width, rs.height}, "test", sf::Style::Titlebar | sf::Style::Close);
    } catch (...) {
        GTEST_SKIP() << "SFML window creation failed";
    }
    std::unique_ptr<sf::RenderWindow> window(wnd);
    if (!window->isOpen()) {
        GTEST_SKIP() << "SFML window not open";
    }

    sf::Image image;
    image.create(rs.width, rs.height);
    sf::Texture texture;
    texture.create(rs.width, rs.height);
    sf::Sprite sprite;

    RenderResult rr;
    rr.settings = rs;
    rr.color_data.resize(rs.height, std::vector<mandelbrot::RgbColor>(rs.width));

    for (unsigned y = 0; y < rs.height; ++y)
        for (unsigned x = 0; x < rs.width; ++x)
            rr.color_data[y][x] = mandelbrot::RgbColor{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 0};

    testutil::ValueHolder<> holder{};
    auto sender = SFMLRender(rr, image, texture, sprite, *window, rs);
    auto op = stdexec::connect(sender, testutil::Receiver<decltype(holder)>{&holder});
    stdexec::start(op);

    auto back = texture.copyToImage();
    auto pix = back.getPixel(3, 5);
    EXPECT_EQ(pix.r, 3u);
    EXPECT_EQ(pix.g, 5u);
}

// --------------------- SfmlEventHandler tests ---------------------
TEST(SfmlEventHandlerTest, ContinuousZoomChangesViewport) {
    auto rs = SmallSettings(100, 80);
    sf::RenderWindow *wnd = nullptr;
    try {
        wnd = new sf::RenderWindow(sf::VideoMode{rs.width, rs.height}, "test", sf::Style::Titlebar | sf::Style::Close);
    } catch (...) {
        GTEST_SKIP() << "SFML window creation failed";
    }
    std::unique_ptr<sf::RenderWindow> window(wnd);
    if (!window->isOpen()) {
        GTEST_SKIP() << "SFML window not open";
    }

    AppState state;
    sf::Clock clk;
    SfmlEventHandler handler(*window, rs, state, clk);

    sf::Mouse::setPosition(sf::Vector2i{static_cast<int>(rs.width / 2), static_cast<int>(rs.height / 2)}, *window);
    state.left_mouse_pressed = true;
    std::this_thread::sleep_for(120ms);

    testutil::ValueHolder<> holder{};
    auto op = stdexec::connect(handler, testutil::Receiver<decltype(holder)>{&holder});
    auto vp_before = state.viewport;
    stdexec::start(op);
    auto vp_after = state.viewport;

    EXPECT_LT(vp_after.width(), vp_before.width());
    EXPECT_LT(vp_after.height(), vp_before.height());
}

// --------------------- WaitForFPS test ---------------------
TEST(WaitForFPSTest, SleepsToMaintainTarget) {
    FrameClock clock;
    WaitForFPS limiter(clock, 50);
    auto t0 = std::chrono::steady_clock::now();
    limiter();
    auto dt = std::chrono::steady_clock::now() - t0;
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count(), 10);
}

// --------------------- Integration-like test ---------------------
TEST(Integration, ComputeAndRenderOneFrame) {
    auto rs = SmallSettings(40, 30, 40);
    MandelbrotRenderer renderer(4);
    AppState state;
    state.need_rerender = true;

    sf::RenderWindow *wnd = nullptr;
    try {
        wnd = new sf::RenderWindow(sf::VideoMode{rs.width, rs.height}, "test", sf::Style::Titlebar | sf::Style::Close);
    } catch (...) {
        GTEST_SKIP() << "SFML unavailable";
    }
    std::unique_ptr<sf::RenderWindow> window(wnd);
    if (!window->isOpen()) {
        GTEST_SKIP() << "SFML window not open";
    }

    sf::Image image;
    image.create(rs.width, rs.height);
    sf::Texture texture;
    texture.create(rs.width, rs.height);
    sf::Sprite sprite;

    auto pipeline = stdexec::just() |
                    stdexec::let_value([&]() { return CalculateMandelbrotAsyncSender(state, rs, renderer); }) |
                    stdexec::let_value([&](RenderResult data) {
                        return SFMLRender(std::move(data), image, texture, sprite, *window, rs);
                    });

    auto res = stdexec::sync_wait(std::move(pipeline));
    ASSERT_TRUE(res.has_value());
    auto back = texture.copyToImage();
    auto pix = back.getPixel(0, 0);
    EXPECT_TRUE(back.getSize().x == rs.width && back.getSize().y == rs.height);
}

TEST(Integration, PipelineWithRerender) {
    AppState state;
    state.need_rerender = true;
    state.should_exit = true;
    MandelbrotRenderer renderer;
    RenderSettings settings{20, 20, 10, 2.0};

    sf::RenderWindow mock_window(sf::VideoMode(20, 20), "Test");
    sf::Clock mock_zoom;
    sf::Image mock_image;
    mock_image.create(20, 20);
    sf::Texture mock_texture;
    mock_texture.create(20, 20);
    sf::Sprite mock_sprite;

    FrameClock frame_clock;

    auto pipeline =
        SfmlEventHandler{mock_window, settings, state, mock_zoom} |
        stdexec::let_value([&]() { return CalculateMandelbrotAsyncSender{state, settings, renderer}; }) |
        stdexec::let_value([&](RenderResult data) {
            return SFMLRender{std::move(data), mock_image, mock_texture, mock_sprite, mock_window, settings};
        }) |
        stdexec::then(WaitForFPS{frame_clock, 60});

    auto repeated =
        std::move(pipeline) | stdexec::then([&]() { return state.should_exit; }) | exec::repeat_effect_until();

    stdexec::sync_wait(std::move(repeated));

    sf::Color pixel = mock_image.getPixel(10, 10);
    EXPECT_EQ(pixel.r, 0);
    EXPECT_EQ(pixel.g, 0);
    EXPECT_EQ(pixel.b, 0);
}