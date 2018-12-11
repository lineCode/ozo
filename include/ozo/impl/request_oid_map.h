#pragma once

#include <ozo/impl/async_request.h>
#include <ozo/time_traits.h>

namespace ozo::impl {

template <typename T>
inline T unwrap_type_c(hana::basic_type<T>);

template <typename Impl>
inline auto get_types_names(const oid_map_t<Impl>& oid_map) {
    std::vector<std::string> retval;
    retval.reserve(hana::length(oid_map.impl));
    hana::for_each(hana::keys(oid_map.impl), [&] (const auto& key) {
        retval.push_back(type_name<decltype(unwrap_type_c(key))>());
    });
    return retval;
}

template <typename Impl>
inline auto make_oids_query(const oid_map_t<Impl>& oid_map) {
    using namespace literals;
    return "SELECT COALESCE(to_regtype(f)::oid, 0) AS oid FROM UNNEST("_SQL
        + get_types_names(oid_map) + ") AS f"_SQL;
}

using oids_result = std::vector<oid_t>;

template <typename Impl>
inline void set_oid_map(oid_map_t<Impl>& oid_map, const oids_result& res) {
    if (hana::length(oid_map.impl).value != res.size()) {
        throw std::length_error(std::string("result size ")
            + std::to_string(res.size())
            + " does not match to oid map size "
            + std::to_string(hana::length(oid_map.impl)));
    }

    auto i = res.begin();

    hana::for_each(hana::keys(oid_map.impl), [&] (const auto& key) {
        if (*i == null_oid) {
            throw std::invalid_argument(
                std::string("null oid for type ")
                + boost::core::demangle(typeid(key).name()));
        }
        oid_map.impl[key] = *i;
        ++i;
    });
}

template <typename Handler>
struct request_oid_map_op {
    Handler handler_;
    std::shared_ptr<oids_result> res_;

    template <typename Connection>
    void perform(Connection&& conn) {
        const auto oid_map = get_oid_map(conn);
        auto ctx = make_request_operation_context(std::forward<Connection>(conn), *this);
        async_send_query_params(ctx, make_oids_query(oid_map));
        async_get_result(std::move(ctx), async_request_out_handler{std::back_inserter(*res_)});
    }

    template <typename Connection>
    void operator() (error_code ec, Connection&& conn) {
        if (!ec) try {
            set_oid_map(get_oid_map(conn), *res_);
        } catch (const std::exception& e) {
            set_error_context(conn, e.what());
            ec = error::oid_request_failed;
        }
        handler_(ec, std::forward<Connection>(conn));
    }

    using executor_type = decltype(asio::get_associated_executor(handler_));

    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler_);
    }

    using allocator_type = decltype(asio::get_associated_allocator(handler_));

    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler_);
    }
};

template <typename Handler>
auto make_async_request_oid_map_op(Handler&& handler) {
    auto allocator = asio::get_associated_allocator(handler);
    return request_oid_map_op<std::decay_t<Handler>> {
        std::forward<Handler>(handler),
        std::allocate_shared<oids_result>(allocator)
    };
}

} // namespace ozo::impl
