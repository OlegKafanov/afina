#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <iostream>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
        char stack_end;
        //не удалять просто
        //if (std::get<0>(ctx.Stack) != nullptr)
        //        delete[] std::get<0>(ctx.Stack);

        //куда растет стек
        ctx.Low = std::min(&stack_end, StackBottom);
        ctx.Hight = std::max(&stack_end, StackBottom);
        uint32_t len = ctx.Hight - ctx.Low;

        ctx.Stack = std::make_tuple(new char[len], len);
        //куда растет стек 2
        memcpy(std::get<0>(ctx.Stack), ctx.Low, len);
}

void Engine::Restore(context &ctx) {
        char stack_end;

        if ((ctx.Low < &stack_end) && (&stack_end < ctx.Hight))
            this->Restore(ctx);

        memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));

        longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    //проверить, что не на текущую
    if (alive != nullptr) {
        if (alive == cur_routine) {
            alive = alive->next;
        }
    }
    if (alive != nullptr) {
        context *ctx = alive;
        alive = alive->next;
        return sched(ctx);
    }
}

void Engine::sched(void *routine_) {
    context *ctx = static_cast<context*>(routine_);
    //проверить, что не текущую
    if (cur_routine != routine_){
        if (cur_routine != nullptr) {
            Store(*cur_routine);
            if (setjmp(cur_routine->Environment) > 0) {
                return;
            }
        }
        cur_routine = ctx;
        Restore(*ctx);
    }
}

//idle контекст: зачем нужен -вопрос. Когда закончится функция, размотка стека,

} // namespace Coroutine
} // namespace Afina
