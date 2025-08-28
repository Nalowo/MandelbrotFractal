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
        const std::uint32_t rows_per_task = 0;
        return /* Sender */;
        /*
        1. Разделите экран на N полос, чтобы каждый пиксель находился только в одной из N областей
        2. Запланируйте (schedule) выполнение MandelbrotSender сендера на thread_pool_
        3. Следующей операцией в цепочке сендеров необходимо реализовать преобразование полученного результата в двумерный массив цветов для заданной области пикселей
        4. Используйте техники fold-expr и раскрытие пачки параметров для объединения всех созданных сендеров в новом сендере stdexec::when_all
        5. После завершения выполнения stdexec::when_all - объедените результаты вычисления цветов на экране для каждого пикселя в структуру RenderResult

        Важно: функция RenderAsync должна лишь возвращать сендер, а его непосредственный запуск должен производиться в сенлдере CalculateMandelbrotAsyncSender
        */
    }
};
