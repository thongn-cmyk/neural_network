#ifndef __DATASOURCE_H__
#define __DATASOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include <variant>
#include <serializer/compact_serializer.h>

namespace datasource
{
    struct SourceDescription
    {
        std::variant<stdx::reflectible_monostate, datasource::file_source::SourceDescription, datasource::s3_source::SourceDescription> src;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(src);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(src);
        }
    };

    struct RetryPolicy
    {
        uint64_t retry_count;
        std::chrono::nanoseconds retry_base;
        std::chrono::nanoseconds max_timeout_dur;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(retry_count, retry_base, max_timeout_dur);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(retry_count, retry_base, max_timeout_dur);
        }
    };

    struct TransactionGetPolicy
    {
        uint64_t transactionable_sz;
        RetryPolicy retry_policy;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(transactionable_sz, retry_policy);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(transactionable_sz, retry_policy);
        }
    };

    struct DetachPullPolicy
    {
        uint64_t white_tx_sz;
        TransactionGetPolicy tx_get_policy;

        std::chrono::nanoseconds chk_dur;
        std::optional<std::chrono::nanoseconds> max_timeout_dur;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(white_tx_sz, tx_get_policy, chk_dur, max_timeout_dur);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(white_tx_sz, tx_get_policy, chk_dur, max_timeout_dur);
        }
    };

    struct WaitPullPolicy
    {
        uint64_t white_tx_sz;
        TransactionGetPolicy tx_get_policy;

        std::optional<std::chrono::nanoseconds> max_timeout_dur;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(white_tx_sz, tx_get_policy, max_timeout_dur);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(white_tx_sz, tx_get_policy, max_timeout_dur);
        }
    };

    struct SourcePullPolicy
    {
        std::variant<stdx::reflectible_monostate, DetachPullPolicy, WaitPullPolicy> policy;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(policy);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(policy);
        }
    };

    struct Configuration
    {
        SourceDescription src;
        SourcePullPolicy policy;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(src, policy);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(src, policy);
        }
    };
}

#endif