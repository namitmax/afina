#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

Engine::~Engine() {
    if (StackBottom) {
        delete [] std::get<0>(idle_ctx->Stack);
        delete idle_ctx;
    }

    while (alive) {
        context* tmp_ctx = alive;
        delete [] std::get<0>(alive->Stack);
        delete tmp_ctx;
        alive = alive->next;
    }

    while (blocked) {
        context* tmp_ctx = blocked;
        delete [] std::get<0>(blocked->Stack);
        delete tmp_ctx;
        blocked = blocked->next;
    }
}

void Engine::Store(context &ctx) {
    char addr;
    if (&addr > StackBottom) {
        ctx.Hight = &addr;
    } else {
        ctx.Low = &addr;
    }

    size_t memory_size = ctx.Hight - ctx.Low;
    if (std::get<1>(ctx.Stack) < memory_size || std::get<1>(ctx.Stack) > 2 * memory_size) {
        delete [] std::get<0>(ctx.Stack);
        std::get<1>(ctx.Stack) = memory_size;
        std::get<0>(ctx.Stack) = new char[memory_size];
    }

    memcpy(std::get<0>(ctx.Stack), ctx.Low, memory_size);
}

void Engine::Restore(context &ctx) {
    char addr;
    while (&addr >= ctx.Low && &addr <= ctx.Hight) {
        Restore(ctx);
    }

    memcpy(ctx.Low, std::get<0> (ctx.Stack), ctx.Hight - ctx.Low);
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (!alive || (cur_routine == alive && !alive->next)) {
        return;
    }
    context* ctx = alive;
    if (ctx && ctx == cur_routine) {
        ctx = ctx->next;
    }
    if (ctx) {
        sched(ctx);
    }
}

void Engine::sched(void *routine_) {
    context *routine = static_cast<context *>(routine_);
    if (routine_ == nullptr) {
        yield();
    }
    if (cur_routine == routine || routine->blocked) {
        return;
    }
    if (cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    Restore(*routine);
}

void Engine::block(void *_coro) {
    context* coro = static_cast<context*> (_coro);
    if (!_coro) {
        coro = cur_routine;
    }

    if (coro->blocked || !coro) {
        return;
    }
    coro->blocked = true;

    if (alive == coro) {
        alive = alive->next;
    }
    if (coro->prev) {
        coro->prev->next = coro->next;
    }
    if (coro->next) {
        coro->next->prev = coro->prev;
    }
    coro->prev = nullptr;
    coro->next = blocked;
    blocked = coro;
    if (blocked->next) {
        blocked->next->prev = coro;
    }
    if (coro == cur_routine) {
        if (cur_routine != idle_ctx) {
            if (setjmp(cur_routine->Environment) > 0) {
                return;
            }
            Store(*cur_routine);
        }
        Restore(*idle_ctx);
    }
}

void Engine::unblock(void *_coro) {
    context* coro = static_cast<context*>(_coro);

    if (!coro || !coro->blocked) {
        return;
    }
    coro->blocked = false;

    if (blocked == coro) {
        blocked = blocked->next;
    }
    if (coro->prev) {
        coro->prev->next = coro->next;
    }
    if (coro->next) {
        coro->next->prev = coro->prev;
    }
    coro->prev = nullptr;
    coro->next = alive;
    alive = coro;
    if (alive->next) {
        alive->next->prev = coro;
    }

}

} // namespace Coroutine

} // namespace Afina
