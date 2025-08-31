#pragma once

#include "mandelbrot_renderer.hpp"
#include <stdexec/execution.hpp>
#include <tuple>

class CalculateMandelbrotAsyncSender {
public:
    using sender_concept = stdexec::sender_t;

    explicit CalculateMandelbrotAsyncSender(AppState &state, RenderSettings render_settings,
                                            MandelbrotRenderer &renderer)
        : state_(state), render_settings_{render_settings}, renderer_{renderer} {}

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(RenderResult),
                                              stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_stopped_t()>{};
    }

    template <typename Receiver>
    struct OpState {
        Receiver receiver_;
        RenderSettings render_settings_;
        MandelbrotRenderer &renderer_;
        AppState &state_;

        OpState(Receiver &&r, RenderSettings rs, MandelbrotRenderer &renderer, AppState &state)
            : receiver_(std::forward<Receiver>(r)), render_settings_(rs), renderer_(renderer), state_(state) {}

        void start() noexcept {
            try {
                // Если перерисовки не требуется — возвращаем пустой RenderResult
                if (!state_.need_rerender) {
                    RenderResult empty;
                    empty.settings = render_settings_;
                    empty.viewport = state_.viewport;
                    stdexec::set_value(std::move(receiver_), std::move(empty));
                    return;
                }

                auto sender_render = renderer_.RenderAsync<THREAD_POOL_SIZE>(state_.viewport, render_settings_);
                auto tup = stdexec::sync_wait(std::move(sender_render)).value();
                auto rr = std::get<0>(tup);

                state_.need_rerender = false;

                stdexec::set_value(std::move(receiver_), std::move(rr));
            } catch (...) {
                stdexec::set_error(std::move(receiver_), std::current_exception());
            }
        }
    };

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
        return OpState{std::forward<Receiver>(receiver), render_settings_, renderer_, state_};
    }

private:
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
    AppState &state_;
};
