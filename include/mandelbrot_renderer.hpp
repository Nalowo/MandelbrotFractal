#pragma once

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "types.hpp"

class MandelbrotRenderer {
private:
    exec::static_thread_pool thread_pool_;

public:
    explicit MandelbrotRenderer(std::uint32_t num_threads = std::thread::hardware_concurrency())
        : thread_pool_{num_threads} {}

    template <size_t N>
    [[nodiscard]] auto RenderAsync(mandelbrot::ViewPort viewport, RenderSettings settings) {
        /*
        1. Разделите экран на N полос, чтобы каждый пиксель находился только в одной из N областей
        2. Запланируйте (schedule) выполнение MandelbrotSender сендера на thread_pool_
        3. Следующей операцией в цепочке сендеров необходимо реализовать преобразование полученного результата в
        двумерный массив цветов для заданной области пикселей
        4. Используйте техники fold-expr и раскрытие пачки параметров для объединения всех созданных сендеров в новом
        сендере stdexec::when_all
        5. После завершения выполнения stdexec::when_all - объедените результаты вычисления цветов на экране для каждого
        пикселя в структуру RenderResult

        Важно: функция RenderAsync должна лишь возвращать сендер, а его непосредственный запуск должен производиться в
        сенлдере CalculateMandelbrotAsyncSender
        */
        auto sched = thread_pool_.get_scheduler();

        std::array<PixelRegion, N> regions;
        const std::uint32_t strip_height = settings.height / N;
        const std::uint32_t remainder = settings.height % N;
        std::uint32_t current_row = 0;
        
        for (size_t i = 0; i < N; ++i) {
            std::uint32_t height = strip_height + (i < remainder ? 1 : 0);
            regions[i] = {current_row, current_row + height, 0, settings.width};
            current_row += height;
        }

        auto create_when_all = [&]<size_t... I>(std::index_sequence<I...>) {
            return stdexec::when_all((stdexec::on(sched, MakeMandelbrotSender(viewport, settings, regions[I])))...);
        };

        auto all_senders = create_when_all(std::make_index_sequence<N>{});

        return all_senders | stdexec::then([regions, viewport, settings](auto &&...matrices) {
                   RenderResult result;
                   result.viewport = viewport;
                   result.settings = settings;
                   result.pixel_data.resize(settings.height, std::vector<std::uint32_t>(settings.width));
                   result.color_data.resize(settings.height, std::vector<mandelbrot::RgbColor>(settings.width));

                   size_t index = 0;
                   (
                       [&](auto &&mat) {
                           const auto &reg = regions[index++];
                           for (std::uint32_t py = 0; py < mat.size(); ++py) {
                               std::uint32_t y = reg.start_row + py;
                               for (std::uint32_t px = 0; px < mat[py].size(); ++px) {
                                   std::uint32_t x = reg.start_col + px;
                                   std::uint32_t it = mat[py][px];
                                   result.pixel_data[y][x] = it;
                                   result.color_data[y][x] = mandelbrot::IterationsToColor(it, settings.max_iterations);
                               }
                           }
                       }(std::move(matrices)),
                       ...);
                   return result;
               });
    }
};
