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

        /* Ваш код здесь */
    };

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
};
