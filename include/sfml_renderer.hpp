#pragma once

#include <SFML/Graphics.hpp>
#include <print>
#include <stdexec/execution.hpp>

#include "types.hpp"

class SFMLRender {
public:
    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        RenderResult render_result_;
        sf::Image &image_;
        sf::Texture &texture_;
        sf::Sprite &sprite_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;

        OperationState(Receiver &&r, RenderResult rr, sf::Image &image, sf::Texture &texture, sf::Sprite &sprite,
                       sf::RenderWindow &window, RenderSettings render_settings)
            : receiver_{std::forward<Receiver>(r)}, render_result_{std::move(rr)}, image_{image}, texture_{texture},
              sprite_{sprite}, window_{window}, render_settings_{render_settings} {}

        void start() noexcept {
            try {
                if (render_result_.color_data.empty()) {
                    // ничего не рисуем, просто сигнализируем, что работа завершена
                    stdexec::set_value(std::move(receiver_));
                    return;
                }

                const std::uint32_t height = render_settings_.height;
                const std::uint32_t width = render_settings_.width;

                for (std::uint32_t r = 0; r < height; ++r) {
                    for (std::uint32_t c = 0; c < width; ++c) {
                        const auto &col = render_result_.color_data[r][c];
                        image_.setPixel(static_cast<unsigned>(c), static_cast<unsigned>(r),
                                        sf::Color(col.r, col.g, col.b));
                    }
                }

                texture_.update(image_);
                sprite_.setTexture(texture_, true);

                window_.clear();
                window_.draw(sprite_);
                window_.display();

                stdexec::set_value(std::move(receiver_));
            } catch (...) {
                stdexec::set_error(std::move(receiver_), std::current_exception());
            }
        }
    };

    using sender_concept = stdexec::sender_t;

    RenderResult render_result_;
    sf::Image &image_;
    sf::Texture &texture_;
    sf::Sprite &sprite_;
    sf::RenderWindow &window_;
    RenderSettings render_settings_;

    SFMLRender(RenderResult render_result, sf::Image &image, sf::Texture &texture, sf::Sprite &sprite,
               sf::RenderWindow &window, RenderSettings render_settings)
        : render_result_(render_result), image_{image}, texture_{texture}, sprite_{sprite}, window_{window},
          render_settings_{render_settings} {}

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
        return OperationState<std::decay_t<Receiver>>{
            std::forward<Receiver>(receiver), render_result_, image_, texture_, sprite_, window_, render_settings_};
    }

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_stopped_t()>{};
    }
};
