#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
        volatile char stack_end;
        if (std::get<0>(ctx.Stack) != nullptr)
                delete[] std::get<0>(ctx.Stack);
        uint32_t len = StackBottom - &stack_end;
        ctx.Stack = std::make_tuple(new char[len], len);
        memcpy(std::get<0>(ctx.Stack), (const void*)&stack_end, len);
}

void Engine::Restore(context &ctx) {
        volatile char stack_end;
        if (&stack_end > StackBottom - std::get<1>(ctx.Stack))
                Restore(ctx);

        memcpy(StackBottom - std::get<1>(ctx.Stack),
                std::get<0>(ctx.Stack),
                std::get<1>(ctx.Stack));

        longjmp(ctx.Environment, 1);
}

void Engine::yield() {
        if (alive != nullptr) {
                context *ctx = alive;
                alive = alive->next;
                return sched(ctx);
        }
}

void Engine::sched(void *routine_) {
        context *ctx = (context*)routine_;

        if (cur_routine != nullptr) {
                Store(*cur_routine);
                 if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
    }
    cur_routine = ctx;
    Restore(*ctx);
}

} // namespace Coroutine
} // namespace Afina
