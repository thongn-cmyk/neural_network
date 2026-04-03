#ifndef __NETWORK_KERNEL_MAILBOX_IMPL1_SSL_X_H__
#define __NETWORK_KERNEL_MAILBOX_IMPL1_SSL_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <>

namespace dg_sock::network_kernel_mailbox_impl1_ssl_x
{
    //in this secure socket layer, it's a mandatory abstract layer, non-optional, build-included, just to not complexify the faults of the upper layers

    struct SSLMessage
    {
        dg_sock::string secret;
        dg_sock::string pad;
        dg_sock::string content;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(secret, pad, content);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(secret, pad, content);
        }
    };
}

#endif