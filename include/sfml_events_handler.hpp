#pragma once

#include <SFML/Graphics.hpp>
#include <stdexec/execution.hpp>

#include "types.hpp"

class SfmlEventHandler {
public:
    using sender_concept = stdexec::sender_t;

    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;
        AppState &state_;
        sf::Clock &zoom_clock_;

        static constexpr float ZOOM_INTERVAL_MS = 100.0f;

        template <typename R>
        explicit OperationState(R &&r, sf::RenderWindow &window, RenderSettings render_settings, AppState &state,
                                sf::Clock &zoom_clock)
            : receiver_{std::forward<R>(r)}, window_{window}, render_settings_{render_settings}, state_{state},
              zoom_clock_{zoom_clock} {}

        void start() noexcept {
            try {
                HandleEvents();
                HandleContinuousZoom();
                if (state_.should_exit)
                    stdexec::set_stopped(std::move(receiver_));
                else
                    stdexec::set_value(std::move(receiver_));
            } catch (...) {
                stdexec::set_error(std::move(receiver_), std::current_exception());
            }
        }

    private:
        void HandleEvents() {
            sf::Event event;
            while (window_.pollEvent(event)) {
                switch (event.type) {
                case sf::Event::Closed:
                    state_.should_exit = true;
                    break;
                case sf::Event::MouseButtonPressed:
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        state_.left_mouse_pressed = true;
                        state_.need_rerender = true;
                    } else if (event.mouseButton.button == sf::Mouse::Right) {
                        state_.right_mouse_pressed = true;
                        state_.need_rerender = true;
                    }
                    break;
                case sf::Event::MouseButtonReleased:
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        state_.left_mouse_pressed = false;
                    } else if (event.mouseButton.button == sf::Mouse::Right) {
                        state_.right_mouse_pressed = false;
                    }
                    break;
                default:
                    break;
                }
            }
        }

        void HandleContinuousZoom() {
            if ((state_.left_mouse_pressed || state_.right_mouse_pressed) &&
                zoom_clock_.getElapsedTime().asMilliseconds() >= ZOOM_INTERVAL_MS) {

                auto mouse_pos = sf::Mouse::getPosition(window_);

                if (mouse_pos.x >= 0 && mouse_pos.x < static_cast<int>(render_settings_.width) && mouse_pos.y >= 0 &&
                    mouse_pos.y < static_cast<int>(render_settings_.height)) {

                    ZoomToPoint(mouse_pos.x, mouse_pos.y, state_.left_mouse_pressed);
                    zoom_clock_.restart();
                }
            }
        }

        void ZoomToPoint(int pixel_x, int pixel_y, bool zoom_in, double factor = 0.8) {
            const double target_x = state_.viewport.x_min +
                                    (static_cast<double>(pixel_x) / render_settings_.width) * state_.viewport.width();
            const double target_y = state_.viewport.y_min +
                                    (static_cast<double>(pixel_y) / render_settings_.height) * state_.viewport.height();

            const double zoom_factor = zoom_in ? factor : (1.0 / factor);
            const double new_width = state_.viewport.width() * zoom_factor;
            const double new_height = state_.viewport.height() * zoom_factor;

            state_.viewport.x_min = target_x - new_width / 2.0;
            state_.viewport.x_max = target_x + new_width / 2.0;
            state_.viewport.y_min = target_y - new_height / 2.0;
            state_.viewport.y_max = target_y + new_height / 2.0;
        }
    };

    SfmlEventHandler(sf::RenderWindow &window, RenderSettings render_settings, AppState &state, sf::Clock &zoom_clock)
        : window_{window}, render_settings_{render_settings}, state_{state}, zoom_clock_{zoom_clock} {}

    template <typename Receiver>
    auto connect(Receiver &&receiver) {
        return OperationState<std::decay_t<Receiver>>{std::forward<Receiver>(receiver), window_, render_settings_,
                                                      state_, zoom_clock_};
    }

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_stopped_t()
                                              >{};
    }

private:
    sf::RenderWindow &window_;
    RenderSettings render_settings_;
    AppState &state_;
    sf::Clock &zoom_clock_;
};
