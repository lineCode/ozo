#pragma once

#include <ozo/impl/async_start_transaction.h>

namespace ozo::impl {

template <typename T, typename Query, typename CompletionToken,
          typename = Require<ConnectionProvider<T>>>
auto start_transaction(T&& provider, Query&& query,
        const time_traits::duration& timeout, CompletionToken&& token) {
    using signature = void (error_code, transaction<connection_type<T>>);

    async_completion<CompletionToken, signature> init(token);

    async_start_transaction(
        std::forward<T>(provider),
        std::forward<Query>(query),
        timeout,
        init.completion_handler
    );

    return init.result.get();
}

} // namespace ozo::impl
